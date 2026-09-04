#include "DRMeshVoxelCarver.h"

#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "StaticMeshResources.h"
#include "TimerManager.h"
#include "VoxelAsyncWork.h"
#include "VoxelData/VoxelData.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelTools/VoxelToolHelpers.h"
#include "VoxelWorld.h"

namespace DRMeshVoxelCarver
{
	constexpr int32 MaxRetryCount = 100;
	constexpr float RetryInterval = 0.1f;
	const FVector RayDirection = FVector(1.f, 0.137f, 0.071f).GetSafeNormal();

	struct FCarveContext
	{
		FIntVector Min = FIntVector::ZeroValue;
		FIntVector Max = FIntVector::ZeroValue;
		int32 Step = 1;
		int32 ChunkSize = 64;
		FTransform MeshTransform;
		FTransform VoxelTransform;
		float VoxelSize = 100.f;
		FIntVector VoxelWorldOffset = FIntVector::ZeroValue;
		FVoxelValue TargetValue = FVoxelValue::Empty();
		EDRMeshVoxelSampleShape SampleShape = EDRMeshVoxelSampleShape::Cube;
		float SphereRadius = 1.f;
		float RandomOffsetRatio = 0.f;
		float SphereNoiseStrength = 0.f;
		float SphereNoiseScale = 0.15f;
		int32 RandomSeed = 1337;
		FVector NoiseSeedOffset = FVector::ZeroVector;
		float MaximumSphereRadius = 1.f;
		int32 SphereExtent = 0;
		TArray<FVector3f> Vertices;
		TArray<uint32> Indices;
		TArray<FIntVector> ChunkMins;
		int32 NextChunkIndex = 0;
		TWeakObjectPtr<UStaticMeshComponent> CarveMesh;
		TWeakObjectPtr<AVoxelWorld> VoxelWorld;
		bool bHideMeshAfterCarve = true;
		TFunction<void()> Completion;
	};

	bool IsPointInsideMesh(
		const FVector& LocalPoint,
		const TArray<FVector3f>& Vertices,
		const TArray<uint32>& Indices)
	{
		int32 HitCount = 0;
		for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
		{
			const FVector A(Vertices[Indices[Index]]);
			const FVector B(Vertices[Indices[Index + 1]]);
			const FVector C(Vertices[Indices[Index + 2]]);
			const FVector EdgeAB = B - A;
			const FVector EdgeAC = C - A;
			const FVector P = FVector::CrossProduct(RayDirection, EdgeAC);
			const double Determinant = FVector::DotProduct(EdgeAB, P);
			if (FMath::IsNearlyZero(Determinant))
			{
				continue;
			}

			const double InverseDeterminant = 1.0 / Determinant;
			const FVector T = LocalPoint - A;
			const double U = FVector::DotProduct(T, P) * InverseDeterminant;
			if (U < 0.0 || U > 1.0)
			{
				continue;
			}

			const FVector Q = FVector::CrossProduct(T, EdgeAB);
			const double V = FVector::DotProduct(RayDirection, Q) * InverseDeterminant;
			const double Distance = FVector::DotProduct(EdgeAC, Q) * InverseDeterminant;
			if (V >= 0.0 && U + V <= 1.0 && Distance > KINDA_SMALL_NUMBER)
			{
				++HitCount;
			}
		}

		return HitCount % 2 == 1;
	}

	class FMeshVoxelCarveWork final : public FVoxelAsyncWork
	{
	public:
		FMeshVoxelCarveWork(AVoxelWorld& VoxelWorld, TFunction<void(FVoxelData&)>&& InWork)
			: FVoxelAsyncWork(TEXT("DR Mesh Voxel Carve"), 1e9, true)
			, Data(VoxelWorld.GetDataSharedPtr())
			, Work(MoveTemp(InWork))
		{
		}

		virtual uint32 GetPriority() const override
		{
			return 0;
		}

		virtual void DoWork() override
		{
			const auto PinnedData = Data.Pin();
			if (PinnedData.IsValid())
			{
				Work(*PinnedData);
			}
		}

	private:
		TVoxelWeakPtr<FVoxelData> Data;
		TFunction<void(FVoxelData&)> Work;
	};

	void StartNextCarveChunk(const TSharedRef<FCarveContext, ESPMode::ThreadSafe>& Context);
}

ADRMeshVoxelCarver::ADRMeshVoxelCarver()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	CarveMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarveMesh"));
	SetRootComponent(CarveMesh);
	CarveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRMeshVoxelCarver::BeginPlay()
{
	Super::BeginPlay();
	MiningGameState = GetWorld()->GetGameState<ADRMiningGameStateBase>();
	if (MiningGameState.IsValid())
	{
		MiningGameState->OnGamePhaseChanged.AddDynamic(
			this,
			&ThisClass::HandleGamePhaseChanged);
	}

	if (bCarveOnBeginPlay)
	{
		StartCarveBatch(false);
	}
}

void ADRMeshVoxelCarver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MiningGameState.IsValid())
	{
		MiningGameState->OnGamePhaseChanged.RemoveDynamic(
			this,
			&ThisClass::HandleGamePhaseChanged);
	}

	GetWorldTimerManager().ClearTimer(RetryTimerHandle);
	MiningGameState.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADRMeshVoxelCarver::RestartCarveBatch()
{
	GetWorldTimerManager().ClearTimer(RetryTimerHandle);
	PendingCarvers.Reset();
	bStartedForCurrentGame = false;
	ActiveGamePhaseIndex = INDEX_NONE;
}

void ADRMeshVoxelCarver::HandleGamePhaseChanged(
	int32 PhaseIndex,
	int32,
	const TArray<FText>&)
{
	if (!bCarveOnGameStart || bStartedForCurrentGame || PhaseIndex != StartPhaseIndex)
	{
		return;
	}

	bStartedForCurrentGame = true;
	ActiveGamePhaseIndex = PhaseIndex;
	StartCarveBatch(true);
}

void ADRMeshVoxelCarver::StartCarveBatch(bool bForGameStart)
{
	bCarveBatchForGameStart = bForGameStart;
	RetryCount = 0;
	PendingCarverIndex = 0;
	PendingCarvers.Reset();
	GetWorldTimerManager().SetTimer(
		RetryTimerHandle,
		this,
		&ThisClass::TryExecuteCarveBatch,
		DRMeshVoxelCarver::RetryInterval,
		true);
}

bool ADRMeshVoxelCarver::CarveVoxelWorld()
{
	return StartCarveAsync([]() {});
}

bool ADRMeshVoxelCarver::StartCarveAsync(TFunction<void()>&& Completion)
{
	AVoxelWorld* VoxelWorld = ResolveVoxelWorld();
	UStaticMesh* StaticMesh = CarveMesh ? CarveMesh->GetStaticMesh() : nullptr;
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || !IsValid(StaticMesh))
	{
		return false;
	}

	const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Mesh voxel carve failed: %s has no render data."),
			*GetNameSafe(StaticMesh));
		return false;
	}

#if !WITH_EDITOR
	if (!StaticMesh->bAllowCPUAccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Mesh voxel carve failed: enable Allow CPU Access on %s."),
			*GetNameSafe(StaticMesh));
		return false;
	}
#endif

	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	TArray<FVector3f> Vertices;
	Vertices.SetNumUninitialized(LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices());
	for (uint32 Index = 0; Index < LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices(); ++Index)
	{
		Vertices[Index] = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Index);
	}

	const FIndexArrayView SourceIndices = LOD.IndexBuffer.GetArrayView();
	TArray<uint32> Indices;
	Indices.Reserve(SourceIndices.Num());
	for (int32 Index = 0; Index < SourceIndices.Num(); ++Index)
	{
		Indices.Add(SourceIndices[Index]);
	}

	const FBoxSphereBounds MeshBounds = CarveMesh->Bounds;
	const FVector WorldMin = MeshBounds.Origin - MeshBounds.BoxExtent;
	const FVector WorldMax = MeshBounds.Origin + MeshBounds.BoxExtent;
	const FIntVector Min = VoxelWorld->GlobalToLocal(
		WorldMin, EVoxelWorldCoordinatesRounding::RoundDown) - 1;
	const FIntVector Max = VoxelWorld->GlobalToLocal(
		WorldMax, EVoxelWorldCoordinatesRounding::RoundUp) + 1;
	const int32 Step = FMath::Max(1, SamplingStep);
	const int32 ChunkSize = FMath::DivideAndRoundUp(FMath::Max(Step, CarveChunkSize), Step) * Step;
	const int64 SampleCount = int64(FMath::DivideAndRoundUp(Max.X - Min.X, Step))
		* int64(FMath::DivideAndRoundUp(Max.Y - Min.Y, Step))
		* int64(FMath::DivideAndRoundUp(Max.Z - Min.Z, Step));
	if (SampleCount <= 0 || SampleCount > MaxVoxelCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("Mesh voxel carve skipped: %lld samples exceeds limit %d."),
			SampleCount, MaxVoxelCount);
		return false;
	}

	const FTransform MeshTransform = CarveMesh->GetComponentTransform();
	const FTransform VoxelTransform = VoxelWorld->GetTransform();
	const float VoxelSize = VoxelWorld->VoxelSize;
	const FIntVector VoxelWorldOffset = VoxelWorld->GetWorldOffset();
	const FVoxelValue TargetValue = CarveMode == EDRMeshVoxelCarveMode::Add
		? FVoxelValue::Full()
		: FVoxelValue::Empty();
	const EDRMeshVoxelSampleShape SelectedSampleShape = SampleShape;
	const float SphereRadius = Step * FMath::Max(0.5f, SphereOverlap);
	const float SelectedRandomOffsetRatio = FMath::Max(0.f, RandomOffsetRatio);
	const float SelectedSphereNoiseStrength = FMath::Clamp(SphereNoiseStrength, 0.f, 0.75f);
	const float SelectedSphereNoiseScale = FMath::Max(0.01f, SphereNoiseScale);
	const int32 SelectedRandomSeed = RandomSeed;
	const FVector NoiseSeedOffset(
		SelectedRandomSeed * 0.013f,
		SelectedRandomSeed * 0.029f,
		SelectedRandomSeed * 0.047f);
	const float MaximumSphereRadius = SphereRadius * (1.f + SelectedSphereNoiseStrength);
	const int32 SphereExtent = FMath::CeilToInt(
		MaximumSphereRadius + Step * SelectedRandomOffsetRatio);
	TSharedRef<DRMeshVoxelCarver::FCarveContext, ESPMode::ThreadSafe> Context =
		MakeShared<DRMeshVoxelCarver::FCarveContext, ESPMode::ThreadSafe>();
	Context->Min = Min;
	Context->Max = Max;
	Context->Step = Step;
	Context->ChunkSize = ChunkSize;
	Context->MeshTransform = MeshTransform;
	Context->VoxelTransform = VoxelTransform;
	Context->VoxelSize = VoxelSize;
	Context->VoxelWorldOffset = VoxelWorldOffset;
	Context->TargetValue = TargetValue;
	Context->SampleShape = SelectedSampleShape;
	Context->SphereRadius = SphereRadius;
	Context->RandomOffsetRatio = SelectedRandomOffsetRatio;
	Context->SphereNoiseStrength = SelectedSphereNoiseStrength;
	Context->SphereNoiseScale = SelectedSphereNoiseScale;
	Context->RandomSeed = SelectedRandomSeed;
	Context->NoiseSeedOffset = NoiseSeedOffset;
	Context->MaximumSphereRadius = MaximumSphereRadius;
	Context->SphereExtent = SphereExtent;
	Context->Vertices = MoveTemp(Vertices);
	Context->Indices = MoveTemp(Indices);
	Context->CarveMesh = CarveMesh;
	Context->VoxelWorld = VoxelWorld;
	Context->bHideMeshAfterCarve = bHideMeshAfterCarve;
	Context->Completion = MoveTemp(Completion);

	for (int32 X = Min.X; X < Max.X; X += ChunkSize)
	{
		for (int32 Y = Min.Y; Y < Max.Y; Y += ChunkSize)
		{
			for (int32 Z = Min.Z; Z < Max.Z; Z += ChunkSize)
			{
				Context->ChunkMins.Add(FIntVector(X, Y, Z));
			}
		}
	}

	if (bCarveOutsideIn)
	{
		const FVector BoundsCenter = (FVector(Min) + FVector(Max)) * 0.5f;
		const FVector BoundsExtent = (FVector(Max) - FVector(Min)) * 0.5f;
		Context->ChunkMins.Sort([BoundsCenter, BoundsExtent, ChunkSize](
			const FIntVector& Left,
			const FIntVector& Right)
		{
			const FVector LeftCenter = FVector(Left) + FVector(ChunkSize * 0.5f);
			const FVector RightCenter = FVector(Right) + FVector(ChunkSize * 0.5f);
			const FVector SafeExtent(
				FMath::Max(1.f, BoundsExtent.X),
				FMath::Max(1.f, BoundsExtent.Y),
				FMath::Max(1.f, BoundsExtent.Z));
			const FVector LeftDistance = (LeftCenter - BoundsCenter).GetAbs() / SafeExtent;
			const FVector RightDistance = (RightCenter - BoundsCenter).GetAbs() / SafeExtent;
			const float LeftLayer = LeftDistance.GetMax();
			const float RightLayer = RightDistance.GetMax();
			return LeftLayer > RightLayer;
		});
	}

	DRMeshVoxelCarver::StartNextCarveChunk(Context);
	return true;
}

void DRMeshVoxelCarver::StartNextCarveChunk(
	const TSharedRef<FCarveContext, ESPMode::ThreadSafe>& Context)
{
	AVoxelWorld* VoxelWorld = Context->VoxelWorld.Get();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		Context->Completion();
		return;
	}

	if (!Context->ChunkMins.IsValidIndex(Context->NextChunkIndex))
	{
		if (Context->bHideMeshAfterCarve)
		{
			if (UStaticMeshComponent* CarveMesh = Context->CarveMesh.Get())
			{
				CarveMesh->SetVisibility(false, true);
			}
		}
		Context->Completion();
		return;
	}

	const FIntVector ChunkMin = Context->ChunkMins[Context->NextChunkIndex++];
	const FIntVector ChunkMax(
		FMath::Min(ChunkMin.X + Context->ChunkSize, Context->Max.X),
		FMath::Min(ChunkMin.Y + Context->ChunkSize, Context->Max.Y),
		FMath::Min(ChunkMin.Z + Context->ChunkSize, Context->Max.Z));
	const FVoxelIntBox EditedBounds(
		Context->SampleShape == EDRMeshVoxelSampleShape::Sphere
			? ChunkMin - Context->SphereExtent
			: ChunkMin,
		Context->SampleShape == EDRMeshVoxelSampleShape::Sphere
			? ChunkMax + Context->SphereExtent
			: ChunkMax);
	const auto GameThreadTasks = VoxelWorld->GetGameThreadTasks();
	auto Work = [Context, ChunkMin, ChunkMax, EditedBounds, GameThreadTasks](FVoxelData& Data)
	{
		FVoxelWriteScopeLock Lock(Data, EditedBounds, FUNCTION_FNAME);
		for (int32 X = ChunkMin.X; X < ChunkMax.X; X += Context->Step)
		{
			for (int32 Y = ChunkMin.Y; Y < ChunkMax.Y; Y += Context->Step)
			{
				for (int32 Z = ChunkMin.Z; Z < ChunkMax.Z; Z += Context->Step)
				{
					const FIntVector SampleCenter(
						FMath::Min(X + Context->Step / 2, Context->Max.X - 1),
						FMath::Min(Y + Context->Step / 2, Context->Max.Y - 1),
						FMath::Min(Z + Context->Step / 2, Context->Max.Z - 1));
					FIntVector BrushCenter = SampleCenter;
					if (Context->SampleShape == EDRMeshVoxelSampleShape::Sphere &&
						Context->RandomOffsetRatio > 0.f)
					{
						const uint32 PositionHash = HashCombineFast(
							GetTypeHash(SampleCenter), GetTypeHash(Context->RandomSeed));
						FRandomStream RandomStream(PositionHash);
						const float OffsetRange = Context->Step * Context->RandomOffsetRatio;
						BrushCenter += FIntVector(
							FMath::RoundToInt(RandomStream.FRandRange(-OffsetRange, OffsetRange)),
							FMath::RoundToInt(RandomStream.FRandRange(-OffsetRange, OffsetRange)),
							FMath::RoundToInt(RandomStream.FRandRange(-OffsetRange, OffsetRange)));
					}

					const FVector WorldPoint = Context->VoxelTransform.TransformPosition(
						Context->VoxelSize * FVector(BrushCenter + Context->VoxelWorldOffset));
					const FVector LocalPoint = Context->MeshTransform.InverseTransformPosition(WorldPoint);
					if (!IsPointInsideMesh(LocalPoint, Context->Vertices, Context->Indices))
					{
						continue;
					}

					if (Context->SampleShape == EDRMeshVoxelSampleShape::Cube)
					{
						for (int32 BrushX = X; BrushX < FMath::Min(X + Context->Step, Context->Max.X); ++BrushX)
						{
							for (int32 BrushY = Y; BrushY < FMath::Min(Y + Context->Step, Context->Max.Y); ++BrushY)
							{
								for (int32 BrushZ = Z; BrushZ < FMath::Min(Z + Context->Step, Context->Max.Z); ++BrushZ)
								{
									Data.SetValue(BrushX, BrushY, BrushZ, Context->TargetValue);
								}
							}
						}
						continue;
					}

					const int32 BrushExtent = FMath::CeilToInt(Context->MaximumSphereRadius);
					for (int32 OffsetX = -BrushExtent; OffsetX <= BrushExtent; ++OffsetX)
					{
						for (int32 OffsetY = -BrushExtent; OffsetY <= BrushExtent; ++OffsetY)
						{
							for (int32 OffsetZ = -BrushExtent; OffsetZ <= BrushExtent; ++OffsetZ)
							{
								const FIntVector Offset(OffsetX, OffsetY, OffsetZ);
								const FVector NoisePosition = FVector(BrushCenter + Offset)
									* Context->SphereNoiseScale + Context->NoiseSeedOffset;
								const float Noise = FMath::PerlinNoise3D(NoisePosition);
								const float NoisyRadius = Context->SphereRadius
									* (1.f + Noise * Context->SphereNoiseStrength);
								if (FVector(Offset).SizeSquared() <= FMath::Square(NoisyRadius))
								{
									Data.SetValue(BrushCenter + Offset, Context->TargetValue);
								}
							}
						}
					}
				}
			}
		}

		GameThreadTasks->AddTask([Context, EditedBounds]()
		{
			AVoxelWorld* CompletedVoxelWorld = Context->VoxelWorld.Get();
			if (!IsValid(CompletedVoxelWorld) || !CompletedVoxelWorld->IsCreated())
			{
				Context->Completion();
				return;
			}

			FVoxelToolHelpers::UpdateWorld(CompletedVoxelWorld, EditedBounds);
			// 렌더 갱신과 다음 편집이 같은 프레임에 연속 실행되지 않게 한 Chunk씩 넘긴다.
			CompletedVoxelWorld->GetWorldTimerManager().SetTimerForNextTick([Context]()
			{
				StartNextCarveChunk(Context);
			});
		});
	};

	FVoxelToolHelpers::StartAsyncEditTask(
		VoxelWorld,
		new FMeshVoxelCarveWork(*VoxelWorld, MoveTemp(Work)));
}

AVoxelWorld* ADRMeshVoxelCarver::ResolveVoxelWorld()
{
	if (IsValid(TargetVoxelWorld))
	{
		return TargetVoxelWorld;
	}

	for (TActorIterator<AVoxelWorld> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			TargetVoxelWorld = *It;
			return TargetVoxelWorld;
		}
	}

	return nullptr;
}

void ADRMeshVoxelCarver::TryExecuteCarveBatch()
{
	TArray<ADRMeshVoxelCarver*> Carvers;
	for (TActorIterator<ADRMeshVoxelCarver> It(GetWorld()); It; ++It)
	{
		ADRMeshVoxelCarver* Carver = *It;
		if (!IsValid(Carver))
		{
			continue;
		}

		const bool bShouldCarve = bCarveBatchForGameStart
			? Carver->bCarveOnGameStart && Carver->StartPhaseIndex == ActiveGamePhaseIndex
			: Carver->bCarveOnBeginPlay;
		if (bShouldCarve)
		{
			Carvers.Add(Carver);
		}
	}

	Carvers.Sort([](const ADRMeshVoxelCarver& Left, const ADRMeshVoxelCarver& Right)
	{
		if (Left.Priority != Right.Priority)
		{
			return Left.Priority < Right.Priority;
		}
		return Left.GetPathName() < Right.GetPathName();
	});

	if (Carvers.IsEmpty() || Carvers[0] != this)
	{
		GetWorldTimerManager().ClearTimer(RetryTimerHandle);
		return;
	}

	AVoxelWorld* VoxelWorld = ResolveVoxelWorld();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		if (++RetryCount >= DRMeshVoxelCarver::MaxRetryCount)
		{
			GetWorldTimerManager().ClearTimer(RetryTimerHandle);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(RetryTimerHandle);
	PendingCarvers.Reset(Carvers.Num());
	for (ADRMeshVoxelCarver* Carver : Carvers)
	{
		if (IsValid(Carver))
		{
			PendingCarvers.Add(Carver);
		}
	}
	PendingCarverIndex = 0;
	ExecuteNextCarver();
}

void ADRMeshVoxelCarver::ExecuteNextCarver()
{
	while (PendingCarvers.IsValidIndex(PendingCarverIndex))
	{
		ADRMeshVoxelCarver* Carver = PendingCarvers[PendingCarverIndex++].Get();
		if (!IsValid(Carver))
		{
			continue;
		}

		const TWeakObjectPtr<ADRMeshVoxelCarver> WeakThis(this);
		if (Carver->StartCarveAsync([WeakThis]()
		{
			if (ADRMeshVoxelCarver* BatchOwner = WeakThis.Get())
			{
				BatchOwner->ExecuteNextCarver();
			}
		}))
		{
			return;
		}
	}

	PendingCarvers.Reset();
}
