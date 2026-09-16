#include "DRMeshVoxelCarver.h"
#include "Async/Async.h"

#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Core/GameModes/DRMiningGameModeBase.h"
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

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

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
		bool bTrimOutsideMesh = false;
		FTransform CleanupTransform;
		FVector CleanupExtent = FVector::ZeroVector;
		TSet<FIntVector> KeepVoxels;
		TSet<FIntVector> FillVoxels;
		TSharedPtr<FThreadSafeBool, ESPMode::ThreadSafe> Cancellation;
		TFunction<void(bool)> Completion;
	};

	bool IsPointInsideMesh(
		const FVector& LocalPoint,
		const TArray<FVector3f>& Vertices,
		const TArray<uint32>& Indices)
	{
		TArray<double, TInlineAllocator<16>> HitDistances;
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
				HitDistances.Add(Distance);
			}
		}

		// 삼각형의 공유 변에 걸친 교차를 한 번만 센다.
		HitDistances.Sort();
		int32 HitCount = 0;
		double PreviousDistance = -1.0;
		for (double Distance : HitDistances)
		{
			if (!FMath::IsNearlyEqual(Distance, PreviousDistance, 0.0001))
			{
				++HitCount;
				PreviousDistance = Distance;
			}
		}
		return HitCount % 2 == 1;
	}

	FVoxelIntBox GetVoxelBounds(AVoxelWorld* World, const FBox& Bounds)
	{
		FIntVector Min(MAX_int32);
		FIntVector Max(MIN_int32);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Point((Corner & 1) ? Bounds.Max.X : Bounds.Min.X,
				(Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y,
				(Corner & 4) ? Bounds.Max.Z : Bounds.Min.Z);
			const FIntVector Local = World->GlobalToLocal(
				Point, EVoxelWorldCoordinatesRounding::RoundDown);
			Min = FIntVector(FMath::Min(Min.X, Local.X), FMath::Min(Min.Y, Local.Y),
				FMath::Min(Min.Z, Local.Z));
			Max = FIntVector(FMath::Max(Max.X, Local.X + 2), FMath::Max(Max.Y, Local.Y + 2),
				FMath::Max(Max.Z, Local.Z + 2));
		}
		return FVoxelIntBox(Min - 1, Max);
	}

	bool ReadMesh(UStaticMesh* Mesh, TArray<FVector3f>& Vertices, TArray<uint32>& Indices)
	{
		if (!IsValid(Mesh))
		{
			return false;
		}
#if !WITH_EDITOR
		if (!Mesh->bAllowCPUAccess)
		{
			UE_LOG(LogTemp, Error, TEXT("Enable Allow CPU Access on %s."), *GetNameSafe(Mesh));
			return false;
		}
#endif
		const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
		if (!RenderData || RenderData->LODResources.IsEmpty())
		{
			return false;
		}
		const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
		const uint32 VertexCount = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
		Vertices.Reserve(VertexCount);
		for (uint32 Index = 0; Index < VertexCount; ++Index)
		{
			Vertices.Add(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Index));
		}
		const FIndexArrayView SourceIndices = LOD.IndexBuffer.GetArrayView();
		Indices.Reserve(SourceIndices.Num());
		for (int32 Index = 0; Index < SourceIndices.Num(); ++Index)
		{
			if (SourceIndices[Index] >= VertexCount)
			{
				return false;
			}
			Indices.Add(SourceIndices[Index]);
		}
		return Indices.Num() >= 3 && Indices.Num() % 3 == 0;
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

	bool ShouldTrimVoxel(const FCarveContext& Context, const FIntVector& Position)
	{
		const FVector WorldPoint = Context.VoxelTransform.TransformPosition(
			Context.VoxelSize * FVector(Position + Context.VoxelWorldOffset));
		const FVector Local = Context.CleanupTransform.InverseTransformPosition(WorldPoint).GetAbs();
		return Local.X <= Context.CleanupExtent.X && Local.Y <= Context.CleanupExtent.Y
			&& Local.Z <= Context.CleanupExtent.Z && !Context.KeepVoxels.Contains(Position);
	}
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
	RestartCarveBatch();
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
	ClearWorldReadyBindings();
	++BatchGeneration;
	if (CarveCancellation.IsValid())
	{
		*CarveCancellation = true;
		CarveCancellation.Reset();
	}
	GetWorldTimerManager().ClearTimer(RetryTimerHandle);
	PendingCarvers.Reset();
	bBatchSucceeded = true;
	bStartedForCurrentGame = false;
	ActiveGamePhaseIndex = INDEX_NONE;
}

bool ADRMeshVoxelCarver::ShouldCarveOnGameStart(int32 PhaseIndex) const
{
	return bCarveOnGameStart && StartPhaseIndex == PhaseIndex;
}

bool ADRMeshVoxelCarver::IsCarving() const
{
	return (CarveCancellation.IsValid() && !*CarveCancellation)
		|| GetWorldTimerManager().IsTimerActive(RetryTimerHandle)
		|| !WaitingVoxelWorlds.IsEmpty() || !PendingCarvers.IsEmpty();
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
	ClearWorldReadyBindings();
	++BatchGeneration;
	bBatchSucceeded = true;
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
	return StartCarveAsync([](bool) {});
}

bool ADRMeshVoxelCarver::StartCarveAsync(TFunction<void(bool)>&& Completion)
{
	if (CarveCancellation.IsValid() && !*CarveCancellation)
	{
		UE_LOG(LogTemp, Error, TEXT("[Carver] Rejected Actor=%s Phase=%d Reason=AlreadyRunning"),
			*GetPathName(), StartPhaseIndex);
		return false;
	}
	AVoxelWorld* VoxelWorld = ResolveVoxelWorld();
	UStaticMesh* StaticMesh = CarveMesh ? CarveMesh->GetStaticMesh() : nullptr;
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || !IsValid(StaticMesh))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Carver] Rejected Actor=%s Phase=%d World=%s Created=%d Mesh=%s"),
			*GetPathName(), StartPhaseIndex, *GetNameSafe(VoxelWorld),
			IsValid(VoxelWorld) && VoxelWorld->IsCreated(), *GetNameSafe(StaticMesh));
		return false;
	}

	TArray<FVector3f> Vertices;
	TArray<uint32> Indices;
	if (!DRMeshVoxelCarver::ReadMesh(StaticMesh, Vertices, Indices))
	{
		UE_LOG(LogTemp, Error, TEXT("Mesh voxel carve failed: invalid mesh %s."),
			*GetNameSafe(StaticMesh));
		return false;
	}

	const FVoxelIntBox Bounds = DRMeshVoxelCarver::GetVoxelBounds(
		VoxelWorld, CarveMesh->Bounds.GetBox());
	const FIntVector Min = Bounds.Min;
	const FIntVector Max = Bounds.Max;
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
	CarveCancellation = MakeShared<FThreadSafeBool, ESPMode::ThreadSafe>(false);
	Context->Cancellation = CarveCancellation;

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
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || *Context->Cancellation)
	{
		*Context->Cancellation = true;
		Context->Completion(false);
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
		*Context->Cancellation = true;
		Context->Completion(true);
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
		if (*Context->Cancellation)
		{
			GameThreadTasks->AddTask([Context]()
			{
				Context->Completion(false);
			});
			return;
		}
		for (int32 X = ChunkMin.X; X < ChunkMax.X; X += Context->Step)
		{
			for (int32 Y = ChunkMin.Y; Y < ChunkMax.Y; Y += Context->Step)
			{
				for (int32 Z = ChunkMin.Z; Z < ChunkMax.Z; Z += Context->Step)
				{
					if (Context->bTrimOutsideMesh)
					{
						const FIntVector Position(X, Y, Z);
						if (Context->FillVoxels.Contains(Position))
						{
							// 빈 곳만 중립 복셀로 채워 기존 눈과 팀 재질을 보존한다.
							if (Data.GetValue(Position, 0).IsEmpty())
							{
								FVoxelMaterial Material(ForceInit);
								Data.SetMaterial(Position, Material);
								Data.SetValue(Position, FVoxelValue::Full());
							}
						}
						else if (ShouldTrimVoxel(*Context, Position))
						{
							// 다른 거점의 보존 마스크 내부는 변경하지 않는다.
							Data.SetValue(Position, FVoxelValue::Empty());
						}
						continue;
					}
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
			if (!IsValid(CompletedVoxelWorld) || !CompletedVoxelWorld->IsCreated()
				|| *Context->Cancellation)
			{
				*Context->Cancellation = true;
				Context->Completion(false);
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

bool ADRMeshVoxelCarver::BuildMeshVoxelMask(UStaticMeshComponent* Mesh, AVoxelWorld* VoxelWorld,
	int32 MaxSamples, FDRMeshVoxelMask& OutMask)
{
	OutMask = FDRMeshVoxelMask();
	if (!IsValid(Mesh) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}
	TArray<FVector3f> Vertices;
	TArray<uint32> Indices;
	if (!DRMeshVoxelCarver::ReadMesh(Mesh->GetStaticMesh(), Vertices, Indices))
	{
		return false;
	}
	OutMask.Bounds = DRMeshVoxelCarver::GetVoxelBounds(VoxelWorld, Mesh->Bounds.GetBox());
	const FIntVector Size = OutMask.Bounds.Max - OutMask.Bounds.Min;
	const int64 Count = int64(Size.X) * Size.Y * Size.Z;
	if (Count <= 0 || Count > MaxSamples)
	{
		UE_LOG(LogTemp, Error, TEXT("Mesh mask exceeds sample limit: %s (%lld / %d)."),
			*GetNameSafe(Mesh->GetStaticMesh()), Count, MaxSamples);
		return false;
	}
	const FTransform Transform = Mesh->GetComponentTransform();
	for (int32 X = OutMask.Bounds.Min.X; X < OutMask.Bounds.Max.X; ++X)
	{
		for (int32 Y = OutMask.Bounds.Min.Y; Y < OutMask.Bounds.Max.Y; ++Y)
		{
			for (int32 Z = OutMask.Bounds.Min.Z; Z < OutMask.Bounds.Max.Z; ++Z)
			{
				const FIntVector Position(X, Y, Z);
				const FVector Local = Transform.InverseTransformPosition(VoxelWorld->LocalToGlobal(Position));
				if (DRMeshVoxelCarver::IsPointInsideMesh(Local, Vertices, Indices))
				{
					OutMask.InsideVoxels.Add(Position);
				}
			}
		}
	}
	return !OutMask.InsideVoxels.IsEmpty();
}

bool ADRMeshVoxelCarver::BuildMeshVoxelMasksAsync(AVoxelWorld* VoxelWorld,
	const TArray<TPair<UStaticMeshComponent*, int32>>& Meshes,
	const TSharedRef<FThreadSafeBool, ESPMode::ThreadSafe>& Cancellation,
	TFunction<void(TArray<FDRMeshVoxelMask>&&)>&& Completion)
{
	check(IsInGameThread());
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || Meshes.IsEmpty())
	{
		return false;
	}
	struct FMeshInput
	{
		TArray<FVector3f> Vertices;
		TArray<uint32> Indices;
		FTransform Transform;
		FVoxelIntBox Bounds;
	};
	TArray<FMeshInput> Inputs;
	for (const auto& Entry : Meshes)
	{
		if (!IsValid(Entry.Key))
		{
			return false;
		}
		FMeshInput& Input = Inputs.AddDefaulted_GetRef();
		Input.Transform = Entry.Key->GetComponentTransform();
		Input.Bounds = DRMeshVoxelCarver::GetVoxelBounds(VoxelWorld, Entry.Key->Bounds.GetBox());
		const FIntVector Size = Input.Bounds.Max - Input.Bounds.Min;
		const int64 Count = int64(Size.X) * Size.Y * Size.Z;
		if (Count <= 0 || Count > Entry.Value
			|| !DRMeshVoxelCarver::ReadMesh(Entry.Key->GetStaticMesh(), Input.Vertices, Input.Indices))
		{
			return false;
		}
	}
	const FTransform VoxelTransform = VoxelWorld->GetTransform();
	const FIntVector WorldOffset = VoxelWorld->GetWorldOffset();
	const float VoxelSize = VoxelWorld->VoxelSize;
	Async(EAsyncExecution::ThreadPool,
		[Inputs = MoveTemp(Inputs), VoxelTransform, WorldOffset, VoxelSize, Cancellation,
		Completion = MoveTemp(Completion)]() mutable
		{
			const double StartedAt = FPlatformTime::Seconds();
			TArray<FDRMeshVoxelMask> Masks;
			for (const FMeshInput& Input : Inputs)
			{
				FDRMeshVoxelMask& Mask = Masks.AddDefaulted_GetRef();
				Mask.Bounds = Input.Bounds;
				for (int32 X = Input.Bounds.Min.X; X < Input.Bounds.Max.X && !*Cancellation; ++X)
				{
					for (int32 Y = Input.Bounds.Min.Y; Y < Input.Bounds.Max.Y && !*Cancellation; ++Y)
					{
						for (int32 Z = Input.Bounds.Min.Z; Z < Input.Bounds.Max.Z && !*Cancellation; ++Z)
						{
							const FIntVector Position(X, Y, Z);
							const FVector WorldPoint = VoxelTransform.TransformPosition(
								VoxelSize * FVector(Position + WorldOffset));
							const FVector Local = Input.Transform.InverseTransformPosition(WorldPoint);
							if (DRMeshVoxelCarver::IsPointInsideMesh(Local, Input.Vertices, Input.Indices))
							{
								Mask.InsideVoxels.Add(Position);
							}
						}
					}
				}
				if (*Cancellation || Mask.InsideVoxels.IsEmpty())
				{
					Masks.Reset();
					break;
				}
			}
			UE_LOG(LogTemp, Log, TEXT("[ZoneCleanup] Async masks=%d Time=%.3fs Cancelled=%d"),
				Masks.Num(), FPlatformTime::Seconds() - StartedAt, bool(*Cancellation));
			AsyncTask(ENamedThreads::GameThread,
				[Masks = MoveTemp(Masks), Completion = MoveTemp(Completion)]() mutable
				{
					Completion(MoveTemp(Masks));
				});
		});
	return true;
}

bool ADRMeshVoxelCarver::TrimOutsideMesh(AVoxelWorld* VoxelWorld, const FTransform& BoxTransform,
	const FVector& BoxExtent, const FDRMeshVoxelMask& KeepMask, int32 MaxSamples,
	const FDRMeshVoxelMask& FillMask,
	const TSharedRef<FThreadSafeBool, ESPMode::ThreadSafe>& Cancellation,
	TFunction<void(bool)>&& Completion)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || KeepMask.InsideVoxels.IsEmpty())
	{
		return false;
	}
	const FBox WorldBox = FBox(-BoxExtent, BoxExtent).TransformBy(BoxTransform);
	const FVoxelIntBox CleanupVoxelBounds = DRMeshVoxelCarver::GetVoxelBounds(VoxelWorld, WorldBox);
	// 타겟이 정리 Box 밖으로 나와 있어도 내부의 빈 부분은 모두 채운다.
	const FVoxelIntBox Bounds = CleanupVoxelBounds + FillMask.Bounds;
	const FIntVector Size = Bounds.Max - Bounds.Min;
	const int64 Count = int64(Size.X) * Size.Y * Size.Z;
	if (Count <= 0 || Count > MaxSamples)
	{
		UE_LOG(LogTemp, Error, TEXT("Zone cleanup exceeds sample limit (%lld / %d)."), Count, MaxSamples);
		return false;
	}
	const auto Context = MakeShared<DRMeshVoxelCarver::FCarveContext, ESPMode::ThreadSafe>();
	Context->Min = Bounds.Min;
	Context->Max = Bounds.Max;
	Context->VoxelWorld = VoxelWorld;
	Context->VoxelTransform = VoxelWorld->GetTransform();
	Context->VoxelSize = VoxelWorld->VoxelSize;
	Context->VoxelWorldOffset = VoxelWorld->GetWorldOffset();
	Context->bHideMeshAfterCarve = false;
	Context->bTrimOutsideMesh = true;
	Context->CleanupTransform = BoxTransform;
	Context->CleanupExtent = BoxExtent;
	Context->KeepVoxels = KeepMask.InsideVoxels;
	Context->FillVoxels = FillMask.InsideVoxels;
	Context->Cancellation = Cancellation;
	Context->Completion = MoveTemp(Completion);
	// 종료 정리는 Step=1, Cube 고정으로 경계 안쪽을 침범하지 않는다.
	for (int32 X = Bounds.Min.X; X < Bounds.Max.X; X += Context->ChunkSize)
	{
		for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y += Context->ChunkSize)
		{
			for (int32 Z = Bounds.Min.Z; Z < Bounds.Max.Z; Z += Context->ChunkSize)
			{
				Context->ChunkMins.Add(FIntVector(X, Y, Z));
			}
		}
	}
	DRMeshVoxelCarver::StartNextCarveChunk(Context);
	return true;
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

void ADRMeshVoxelCarver::ClearWorldReadyBindings()
{
	for (const TWeakObjectPtr<AVoxelWorld>& World : WaitingVoxelWorlds)
	{
		if (World.IsValid())
		{
			World->OnGenerateWorld.RemoveDynamic(this, &ThisClass::HandleVoxelWorldGenerated);
		}
	}
	WaitingVoxelWorlds.Reset();
	GetWorldTimerManager().ClearTimer(WorldReadyTimeoutHandle);
}

void ADRMeshVoxelCarver::HandleVoxelWorldGenerated()
{
	// 생성 콜스택을 빠져나온 후 모든 대상 월드의 준비 상태를 다시 확인한다.
	GetWorldTimerManager().SetTimer(
		RetryTimerHandle, this, &ThisClass::TryExecuteCarveBatch,
		DRMeshVoxelCarver::RetryInterval, false);
}

void ADRMeshVoxelCarver::HandleWorldReadyTimeout()
{
	ClearWorldReadyBindings();
	GetWorldTimerManager().ClearTimer(RetryTimerHandle);
	bBatchSucceeded = false;
	UE_LOG(LogTemp, Error, TEXT("[Carver] BatchFailed Actor=%s Phase=%d Reason=WorldReadyTimeout"),
		*GetPathName(), ActiveGamePhaseIndex);
	if (bCarveBatchForGameStart)
	{
		if (ADRMiningGameModeBase* Mode = GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>())
		{
			Mode->NotifyPhaseCarversReady(ActiveGamePhaseIndex, false);
		}
	}
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
		ClearWorldReadyBindings();
		GetWorldTimerManager().ClearTimer(RetryTimerHandle);
		return;
	}

	bool bMissingWorld = false;
	bool bWaitingForCreation = false;
	for (ADRMeshVoxelCarver* Carver : Carvers)
	{
		AVoxelWorld* VoxelWorld = Carver->ResolveVoxelWorld();
		if (!IsValid(VoxelWorld))
		{
			bMissingWorld = true;
			continue;
		}
		if (!VoxelWorld->IsCreated())
		{
			bWaitingForCreation = true;
			VoxelWorld->OnGenerateWorld.AddUniqueDynamic(
				this, &ThisClass::HandleVoxelWorldGenerated);
			WaitingVoxelWorlds.AddUnique(VoxelWorld);
		}
	}
	if (bMissingWorld || bWaitingForCreation)
	{
		// 액터가 아직 없을 때만 검색을 재시도한다. 생성 대기는 델리게이트로 재개한다.
		GetWorldTimerManager().ClearTimer(RetryTimerHandle);
		if (bMissingWorld)
		{
			GetWorldTimerManager().SetTimer(
				RetryTimerHandle, this, &ThisClass::TryExecuteCarveBatch,
				DRMeshVoxelCarver::RetryInterval, false);
		}
		if (!GetWorldTimerManager().IsTimerActive(WorldReadyTimeoutHandle))
		{
			GetWorldTimerManager().SetTimer(
				WorldReadyTimeoutHandle, this, &ThisClass::HandleWorldReadyTimeout,
				DRMeshVoxelCarver::RetryInterval * DRMeshVoxelCarver::MaxRetryCount, false);
		}
		return;
	}

	ClearWorldReadyBindings();
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
		const int32 BatchPhase = ActiveGamePhaseIndex;
		const uint32 Generation = BatchGeneration;
		const FString CarverName = Carver->GetPathName();
		if (Carver->StartCarveAsync([WeakThis, BatchPhase, Generation, CarverName](bool bSucceeded)
		{
			if (ADRMeshVoxelCarver* BatchOwner = WeakThis.Get())
			{
				if (BatchOwner->ActiveGamePhaseIndex == BatchPhase && BatchOwner->BatchGeneration == Generation)
				{
					if (!bSucceeded)
					{
						UE_LOG(LogTemp, Error,
							TEXT("[Carver] Interrupted Actor=%s Phase=%d; world lost or work cancelled."),
							*CarverName, BatchPhase);
					}
					BatchOwner->bBatchSucceeded &= bSucceeded;
					BatchOwner->ExecuteNextCarver();
				}
			}
		}))
		{
			return;
		}
		bBatchSucceeded = false;
		UE_LOG(LogTemp, Error, TEXT("[Carver] FailedToStart Actor=%s Phase=%d"),
			*CarverName, BatchPhase);
	}

	PendingCarvers.Reset();
	if (bCarveBatchForGameStart)
	{
		if (ADRMiningGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>())
		{
			GameMode->NotifyPhaseCarversReady(ActiveGamePhaseIndex, bBatchSucceeded);
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDRMeshPreservationTest, "DeepRaiders.GameFlow.MeshPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDRMeshPreservationTest::RunTest(const FString&)
{
	const TArray<FVector3f> Vertices =
	{
		{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
		{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
	};
	const TArray<uint32> Indices =
	{
		0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
		3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5
	};
	TestTrue(TEXT("Closed mesh center is inside"),
		DRMeshVoxelCarver::IsPointInsideMesh(FVector::ZeroVector, Vertices, Indices));
	TestFalse(TEXT("Outside stays outside"),
		DRMeshVoxelCarver::IsPointInsideMesh(FVector(3, 0, 0), Vertices, Indices));
	const FVector VertexRay = FVector(1, 1, 1) - DRMeshVoxelCarver::RayDirection * 0.5;
	TestTrue(TEXT("Shared triangle vertex counts once"),
		DRMeshVoxelCarver::IsPointInsideMesh(VertexRay, Vertices, Indices));

	DRMeshVoxelCarver::FCarveContext Context;
	Context.VoxelSize = 1.f;
	Context.CleanupExtent = FVector(2);
	Context.KeepVoxels.Add(FIntVector::ZeroValue);
	TestFalse(TEXT("Existing mesh interior is never written"),
		DRMeshVoxelCarver::ShouldTrimVoxel(Context, FIntVector::ZeroValue));
	TestTrue(TEXT("Box minus mesh is removed"),
		DRMeshVoxelCarver::ShouldTrimVoxel(Context, FIntVector(1, 0, 0)));
	TestFalse(TEXT("Outside box is never written"),
		DRMeshVoxelCarver::ShouldTrimVoxel(Context, FIntVector(3, 0, 0)));
	Context.CleanupExtent = FVector(2, 0.5, 1);
	Context.CleanupTransform = FTransform(FRotator(0, 90, 0), FVector::ZeroVector);
	TestTrue(TEXT("Rotated box long axis"),
		DRMeshVoxelCarver::ShouldTrimVoxel(Context, FIntVector(0, 1, 0)));
	TestFalse(TEXT("Rotated box short axis"),
		DRMeshVoxelCarver::ShouldTrimVoxel(Context, FIntVector(1, 0, 0)));
	return true;
}
#endif
