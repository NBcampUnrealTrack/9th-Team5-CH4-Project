#include "DRMeshVoxelCarver.h"

#include "Components/StaticMeshComponent.h"
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
	RestartCarveBatch();
}

void ADRMeshVoxelCarver::RestartCarveBatch()
{
	if (!bCarveOnBeginPlay)
	{
		return;
	}

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
	const FVoxelIntBox EditedBounds(
		SampleShape == EDRMeshVoxelSampleShape::Sphere ? Min - SphereExtent : Min,
		SampleShape == EDRMeshVoxelSampleShape::Sphere ? Max + SphereExtent : Max);
	const auto GameThreadTasks = VoxelWorld->GetGameThreadTasks();
	const TWeakObjectPtr<ADRMeshVoxelCarver> WeakThis(this);
	const TWeakObjectPtr<AVoxelWorld> WeakVoxelWorld(VoxelWorld);
	auto Work = [
		Min,
		Max,
		Step,
		MeshTransform,
		VoxelTransform,
		VoxelSize,
		VoxelWorldOffset,
		TargetValue,
		SelectedSampleShape,
		SphereRadius,
		SelectedRandomOffsetRatio,
		SelectedSphereNoiseStrength,
		SelectedSphereNoiseScale,
		SelectedRandomSeed,
		NoiseSeedOffset,
		MaximumSphereRadius,
		SphereExtent,
		EditedBounds,
		Vertices = MoveTemp(Vertices),
		Indices = MoveTemp(Indices),
		GameThreadTasks,
		WeakThis,
		WeakVoxelWorld,
		Completion = MoveTemp(Completion)](FVoxelData& Data) mutable
	{
		FVoxelWriteScopeLock Lock(Data, EditedBounds, FUNCTION_FNAME);
		for (int32 X = Min.X; X < Max.X; X += Step)
		{
			for (int32 Y = Min.Y; Y < Max.Y; Y += Step)
			{
				for (int32 Z = Min.Z; Z < Max.Z; Z += Step)
				{
					const FIntVector SampleCenter(
						FMath::Min(X + Step / 2, Max.X - 1),
						FMath::Min(Y + Step / 2, Max.Y - 1),
						FMath::Min(Z + Step / 2, Max.Z - 1));
					FIntVector BrushCenter = SampleCenter;
					if (SelectedSampleShape == EDRMeshVoxelSampleShape::Sphere
						&& SelectedRandomOffsetRatio > 0.f)
					{
						const uint32 PositionHash = HashCombineFast(
							GetTypeHash(SampleCenter), GetTypeHash(SelectedRandomSeed));
						FRandomStream RandomStream(PositionHash);
						const float OffsetRange = Step * SelectedRandomOffsetRatio;
						BrushCenter += FIntVector(
							FMath::RoundToInt(RandomStream.FRandRange(-OffsetRange, OffsetRange)),
							FMath::RoundToInt(RandomStream.FRandRange(-OffsetRange, OffsetRange)),
							FMath::RoundToInt(RandomStream.FRandRange(-OffsetRange, OffsetRange)));
					}

					const FVector WorldPoint = VoxelTransform.TransformPosition(
						VoxelSize * FVector(BrushCenter + VoxelWorldOffset));
					const FVector LocalPoint = MeshTransform.InverseTransformPosition(WorldPoint);
					if (!DRMeshVoxelCarver::IsPointInsideMesh(LocalPoint, Vertices, Indices))
					{
						continue;
					}

					if (SelectedSampleShape == EDRMeshVoxelSampleShape::Cube)
					{
						for (int32 BrushX = X; BrushX < FMath::Min(X + Step, Max.X); ++BrushX)
						{
							for (int32 BrushY = Y; BrushY < FMath::Min(Y + Step, Max.Y); ++BrushY)
							{
								for (int32 BrushZ = Z; BrushZ < FMath::Min(Z + Step, Max.Z); ++BrushZ)
								{
									Data.SetValue(BrushX, BrushY, BrushZ, TargetValue);
								}
							}
						}
						continue;
					}

					const int32 BrushExtent = FMath::CeilToInt(MaximumSphereRadius);
					for (int32 OffsetX = -BrushExtent; OffsetX <= BrushExtent; ++OffsetX)
					{
						for (int32 OffsetY = -BrushExtent; OffsetY <= BrushExtent; ++OffsetY)
						{
							for (int32 OffsetZ = -BrushExtent; OffsetZ <= BrushExtent; ++OffsetZ)
							{
								const FIntVector Offset(OffsetX, OffsetY, OffsetZ);
								const FVector NoisePosition = FVector(BrushCenter + Offset)
									* SelectedSphereNoiseScale + NoiseSeedOffset;
								const float Noise = FMath::PerlinNoise3D(NoisePosition);
								const float NoisyRadius = SphereRadius
									* (1.f + Noise * SelectedSphereNoiseStrength);
								if (FVector(Offset).SizeSquared() <= FMath::Square(NoisyRadius))
								{
									Data.SetValue(BrushCenter + Offset, TargetValue);
								}
							}
						}
					}
				}
			}
		}

		GameThreadTasks->AddTask([
			EditedBounds,
			WeakThis,
			WeakVoxelWorld,
			Completion = MoveTemp(Completion)]() mutable
		{
			ADRMeshVoxelCarver* Carver = WeakThis.Get();
			AVoxelWorld* CompletedVoxelWorld = WeakVoxelWorld.Get();
			if (IsValid(CompletedVoxelWorld) && CompletedVoxelWorld->IsCreated())
			{
				FVoxelToolHelpers::UpdateWorld(CompletedVoxelWorld, EditedBounds);
			}
			if (IsValid(Carver) && Carver->bHideMeshAfterCarve && IsValid(Carver->CarveMesh))
			{
				Carver->CarveMesh->SetVisibility(false, true);
			}
			Completion();
		});
	};

	FVoxelToolHelpers::StartAsyncEditTask(
		VoxelWorld,
		new DRMeshVoxelCarver::FMeshVoxelCarveWork(*VoxelWorld, MoveTemp(Work)));
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

void ADRMeshVoxelCarver::TryExecuteCarveBatch()
{
	TArray<ADRMeshVoxelCarver*> Carvers;
	for (TActorIterator<ADRMeshVoxelCarver> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->bCarveOnBeginPlay)
		{
			Carvers.Add(*It);
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
