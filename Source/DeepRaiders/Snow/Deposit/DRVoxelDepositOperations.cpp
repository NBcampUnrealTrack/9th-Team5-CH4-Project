#include "DRVoxelDepositOperations.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelIntBox.h"
#include "VoxelMaterial.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelWorld.h"

#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	constexpr float DRDepositMaximumFootprintSlopeDegrees = 55.f;

	struct FDepositDataBatch
	{
		FVoxelIntBoxWithValidity Bounds;
		TArray<int32> Indices;
	};

	template <typename T>
	TMap<FIntVector, FDepositDataBatch> GroupDataBatches(const TArray<T>& Cells, bool bIncludeSupport)
	{
		TMap<FIntVector, FDepositDataBatch> Batches;
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			const FIntVector Position = Cells[Index].Position;
			const FIntVector Key(FMath::FloorToInt(static_cast<double>(Position.X) / 32),
			                     FMath::FloorToInt(static_cast<double>(Position.Y) / 32),
			                     FMath::FloorToInt(static_cast<double>(Position.Z) / 32));
			auto& Batch = Batches.FindOrAdd(Key);
			Batch.Bounds += Position;
			if (bIncludeSupport) { Batch.Bounds += Position - FIntVector(0, 0, 1); }
			Batch.Indices.Add(Index);
		}
		return Batches;
	}

	void UpdateChangedChunks(AVoxelWorld* VoxelWorld, const TArray<FDRVoxelDepositCell>& Cells)
	{
		// Scattered writes must not rebuild the large empty box between drops.
		TMap<FIntVector, FVoxelIntBoxWithValidity> Chunks;
		for (const FDRVoxelDepositCell& Cell : Cells)
		{
			constexpr int32 ChunkSize = 32;
			const FIntVector Chunk(FMath::FloorToInt(static_cast<double>(Cell.Position.X) / ChunkSize),
			                       FMath::FloorToInt(static_cast<double>(Cell.Position.Y) / ChunkSize),
			                       FMath::FloorToInt(static_cast<double>(Cell.Position.Z) / ChunkSize));
			Chunks.FindOrAdd(Chunk) += Cell.Position;
		}
		for (const auto& Chunk : Chunks)
		{
			// Group nearby writes, but never expand a small edit to an entire chunk.
			UVoxelBlueprintLibrary::UpdateBounds(VoxelWorld,
			                                     Chunk.Value.GetBox().Extend(1));
		}
	}

	FVoxelMaterial MakeDepositMaterial(uint8 MaterialIndex)
	{
		// 기본 생성자는 채널을 초기화하지 않습니다. 맵의 기본 눈과 색상/UV를 맞춥니다.
		FVoxelMaterial Material = FVoxelMaterial::CreateFromColor(FLinearColor::Transparent);
		Material.SetSingleIndex(MaterialIndex);
		return Material;
	}

	void GetLocalVoxelBoundsForWorldBox(
		AVoxelWorld* VoxelWorld,
		const FVector& BoxCenter,
		const FVector& AbsExtent,
		FIntVector& OutVoxelMin,
		FIntVector& OutVoxelMax)
	{
		bool bHasBounds = false;
		for (int32 SignX = -1; SignX <= 1; SignX += 2)
		{
			for (int32 SignY = -1; SignY <= 1; SignY += 2)
			{
				for (int32 SignZ = -1; SignZ <= 1; SignZ += 2)
				{
					const FVector WorldCorner = BoxCenter + FVector(
						AbsExtent.X * SignX,
						AbsExtent.Y * SignY,
						AbsExtent.Z * SignZ);
					const FIntVector LocalCorner = VoxelWorld->GlobalToLocal(WorldCorner);
					if (!bHasBounds)
					{
						OutVoxelMin = LocalCorner;
						OutVoxelMax = LocalCorner;
						bHasBounds = true;
						continue;
					}

					OutVoxelMin.X = FMath::Min(OutVoxelMin.X, LocalCorner.X);
					OutVoxelMin.Y = FMath::Min(OutVoxelMin.Y, LocalCorner.Y);
					OutVoxelMin.Z = FMath::Min(OutVoxelMin.Z, LocalCorner.Z);
					OutVoxelMax.X = FMath::Max(OutVoxelMax.X, LocalCorner.X);
					OutVoxelMax.Y = FMath::Max(OutVoxelMax.Y, LocalCorner.Y);
					OutVoxelMax.Z = FMath::Max(OutVoxelMax.Z, LocalCorner.Z);
				}
			}
		}
	}

	bool IsInsideBounds(
		const FIntVector& Position,
		const FIntVector& BoundsMin,
		const FIntVector& BoundsMax)
	{
		return Position.X >= BoundsMin.X && Position.X <= BoundsMax.X &&
			Position.Y >= BoundsMin.Y && Position.Y <= BoundsMax.Y &&
			Position.Z >= BoundsMin.Z && Position.Z <= BoundsMax.Z;
	}

	bool IsSafeInclusiveVoxelBounds(
		const FIntVector& VoxelMin,
		const FIntVector& VoxelMax)
	{
		if (VoxelMin.GetMin() <= MIN_int32 + 64 || VoxelMax.GetMax() >= MAX_int32 - 64)
		{
			return false;
		}

		const int64 SizeX = static_cast<int64>(VoxelMax.X) - VoxelMin.X + 1;
		const int64 SizeY = static_cast<int64>(VoxelMax.Y) - VoxelMin.Y + 1;
		const int64 SizeZ = static_cast<int64>(VoxelMax.Z) - VoxelMin.Z + 1;
		return SizeX > 0 && SizeY > 0 && SizeZ > 0 &&
			SizeX <= MAX_int32 && SizeY <= MAX_int32 && SizeZ <= MAX_int32 &&
			SizeX <= MAX_int32 / SizeY && SizeX * SizeY <= MAX_int32 / SizeZ;
	}

	const FHitResult* FindHighestHit(const TArray<FHitResult>& Hits)
	{
		const FHitResult* TopHit = nullptr;
		for (const FHitResult& Hit : Hits)
		{
			if (!Hit.ImpactPoint.ContainsNaN() &&
				(TopHit == nullptr || Hit.ImpactPoint.Z > TopHit->ImpactPoint.Z))
			{
				TopHit = &Hit;
			}
		}
		return TopHit;
	}

	bool IsEligibleStaticMeshDepositHit(
		const FHitResult& Hit,
		float MinimumSurfaceNormalZ,
		FName RequiredSurfaceTag)
	{
		const UStaticMeshComponent* StaticMeshComponent =
			Cast<UStaticMeshComponent>(Hit.GetComponent());
		if (!IsValid(StaticMeshComponent) ||
			StaticMeshComponent->IsSimulatingPhysics() ||
			Hit.ImpactPoint.ContainsNaN() || Hit.ImpactNormal.ContainsNaN() ||
			Hit.ImpactNormal.Z < MinimumSurfaceNormalZ)
		{
			return false;
		}

		const AActor* HitActor = Hit.GetActor();
		return RequiredSurfaceTag.IsNone() ||
			StaticMeshComponent->ComponentHasTag(RequiredSurfaceTag) ||
			(IsValid(HitActor) && HitActor->ActorHasTag(RequiredSurfaceTag));
	}

	void MakeMeshTraceParameters(
		AActor* TraceOwner,
		AVoxelWorld* VoxelWorld,
		bool bTraceComplex,
		FCollisionObjectQueryParams& OutObjectParams,
		FCollisionQueryParams& OutQueryParams)
	{
		OutObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		// Blueprint map floors can be Movable/WorldDynamic despite remaining stationary.
		OutObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		OutQueryParams = FCollisionQueryParams(
			SCENE_QUERY_STAT(DRStaticMeshDepositSurface),
			bTraceComplex);
		// 복셀 표면은 데이터에서 탐색하므로 갱신이 늦은 복셀 충돌은 제외합니다.
		OutQueryParams.AddIgnoredActor(VoxelWorld);
		if (TraceOwner != nullptr)
		{
			OutQueryParams.AddIgnoredActor(TraceOwner);
		}
	}

	void RecordDepositVoxel(
		FVoxelData& Data,
		const FVoxelMaterial& Material,
		const FIntVector& Position,
		float NewValue,
		TSet<FIntVector>& WrittenPositions,
		FVoxelIntBoxWithValidity& ModifiedBounds,
		TArray<FDRVoxelDepositCell>& ChangedCells,
		bool bPaintMaterial = true)
	{
		const FVoxelValue Value(NewValue);
		if (Data.GetValue(Position, 0) == Value)
		{
			return;
		}
		WrittenPositions.Add(Position);
		Data.SetValue(Position, Value);
		if (bPaintMaterial)
		{
			Data.SetMaterial(Position, Material);
		}
		FDRVoxelDepositCell& Cell = ChangedCells.AddDefaulted_GetRef();
		Cell.Position = Position;
		Cell.Value = Value.GetStorage();
		Cell.bPaintMaterial = bPaintMaterial;
		ModifiedBounds += Position;
	}

	void TryApplyWrite(
		const FDRVoxelDepositPlan& Plan,
		const FDRVoxelDepositWrite& Write,
		FVoxelData& Data,
		const FVoxelMaterial& Material,
		TSet<FIntVector>& WrittenPositions,
		FVoxelIntBoxWithValidity& ModifiedBounds,
		TArray<FDRVoxelDepositCell>& ChangedCells)
	{
		const float DepositAmount = FMath::Min(0.25f,
		                                       Plan.Settings.DepositAmountPerPass * FMath::Max(0.f, Write.AmountScale));
		if (DepositAmount <= SMALL_NUMBER ||
			!IsInsideBounds(Write.Position, Plan.WriteVoxelMin, Plan.WriteVoxelMax) ||
			Write.Position.Z <= Plan.WriteVoxelMin.Z ||
			WrittenPositions.Contains(Write.Position))
		{
			return;
		}

		// 복셀 퇴적은 아래 고체가 필요하며 메시 지지 쓰기는 예외입니다.
		const FIntVector BelowPosition(
			Write.Position.X,
			Write.Position.Y,
			Write.Position.Z - 1);
		const float BelowValue = Data.GetValue(BelowPosition, 0).ToFloat();
		if (BelowValue > 0.f && !Write.bHasStaticMeshSupport)
		{
			return;
		}

		// 이미 고체인 목표 복셀은 건너뜁니다.
		const float CurrentValue = Data.GetValue(Write.Position, 0).ToFloat();
		if (CurrentValue <= 0.f)
		{
			return;
		}

		const float SurfaceHeight = BelowValue <= 0.f
			                            ? BelowPosition.Z + (-BelowValue) / (CurrentValue - BelowValue)
			                            : Write.StaticMeshSurfaceZ;
		const float NewHeight = SurfaceHeight + DepositAmount;
		// 두 셀의 연속적인 표면 값을 함께 낮춥니다. 기존 고체는 재질을 바꾸지 않습니다.
		if (Write.bAllowBelowSupport && !WrittenPositions.Contains(BelowPosition))
		{
			RecordDepositVoxel(Data, Material, BelowPosition,
			                   FMath::Min(BelowValue, FMath::Clamp(BelowPosition.Z - NewHeight, -1.f, 1.f)),
			                   WrittenPositions, ModifiedBounds, ChangedCells, BelowValue > 0.f);
		}
		const float NewValue = FMath::Min(CurrentValue,
		                                  FMath::Clamp(Write.Position.Z - NewHeight, -1.f, 1.f));
		if (!FMath::IsNearlyEqual(CurrentValue, NewValue))
		{
			RecordDepositVoxel(
				Data,
				Material,
				Write.Position,
				NewValue,
				WrittenPositions,
				ModifiedBounds,
				ChangedCells);
		}
	}
}

bool FDRVoxelDepositOperations::PrepareDepositCommand(
	UWorld* World, AVoxelWorld* VoxelWorld, AActor* TraceOwner,
	const FDRVoxelDepositCommand& Command, FDRVoxelDepositPlan& OutPlan)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRDeposit_Prepare);
	OutPlan.Reset();
	const auto& Settings = Command.Settings;
	if (!IsValid(World) || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		Command.AreaCenter.ContainsNaN() || Command.AreaExtent.ContainsNaN() ||
		Command.AreaExtent.GetMin() <= 0 || !FMath::IsFinite(VoxelWorld->VoxelSize) || VoxelWorld->VoxelSize <= 0 ||
		!FMath::IsFinite(Settings.DepositSpreadRadius) || Settings.DepositSpreadRadius < 0 ||
		!FMath::IsFinite(Settings.DepositAmountPerPass) || Settings.DepositAmountPerPass <= 0 ||
		Settings.DropsPerInterval <= 0)
	{
		return false;
	}
	// Vertical data columns require an upright voxel world, just like mesh support heights.
	if (!VoxelWorld->GetActorUpVector().Equals(FVector::UpVector, 0.001f)) { return false; }
	GetLocalVoxelBoundsForWorldBox(VoxelWorld, Command.AreaCenter, Command.AreaExtent,
	                               OutPlan.WriteVoxelMin, OutPlan.WriteVoxelMax);
	if (!IsSafeInclusiveVoxelBounds(OutPlan.WriteVoxelMin, OutPlan.WriteVoxelMax)) { return false; }
	const int64 Depth = static_cast<int64>(OutPlan.WriteVoxelMax.Z) - OutPlan.WriteVoxelMin.Z + 1;
	const double RadiusFloat = Settings.DepositSpreadRadius / VoxelWorld->VoxelSize;
	if (Depth < 2 || Depth > 4096 || RadiusFloat > 16) { return false; }
	const int32 Radius = FMath::RoundToInt(RadiusFloat);
	// Bound both collision queries and exact density reads, independently of area size.
	const int32 ColumnBudget = FMath::Min<int64>(1024, 65536 / Depth);
	const int32 SquareArea = FMath::Square(2 * Radius + 1);
	if (SquareArea > ColumnBudget) { return false; }
	const int32 Drops = FMath::Min(Settings.DropsPerInterval, ColumnBudget / SquareArea);
	FRandomStream Random(Settings.RandomSeed);
	TMap<FIntPoint, float> Columns;
	for (int32 Drop = 0; Drop < Drops; ++Drop)
	{
		const FIntPoint Center(Random.RandRange(OutPlan.WriteVoxelMin.X, OutPlan.WriteVoxelMax.X),
		                       Random.RandRange(OutPlan.WriteVoxelMin.Y, OutPlan.WriteVoxelMax.Y));
		for (int32 X = -Radius; X <= Radius; ++X)
		{
			for (int32 Y = -Radius; Y <= Radius; ++Y)
			{
				const float Distance = FMath::Sqrt(static_cast<float>(X * X + Y * Y));
				if (Distance > Radius + 0.5f) { continue; }
				const FIntPoint XY = Center + FIntPoint(X, Y);
				if (XY.X < OutPlan.WriteVoxelMin.X || XY.X > OutPlan.WriteVoxelMax.X ||
					XY.Y < OutPlan.WriteVoxelMin.Y || XY.Y > OutPlan.WriteVoxelMax.Y) { continue; }
				const float Scale = FMath::Lerp(1.f, 0.55f, Radius > 0 ? FMath::Min(1.f, Distance / Radius) : 0.f);
				float& Existing = Columns.FindOrAdd(XY);
				Existing = FMath::Max(Existing, Scale);
			}
		}
	}
	OutPlan.Settings = Settings;
	FCollisionObjectQueryParams Objects;
	FCollisionQueryParams Query;
	MakeMeshTraceParameters(TraceOwner, VoxelWorld, Command.bTraceComplexStaticMeshSurfaces, Objects, Query);
	// Character meshes must not become persistent terrain supports.
	for (TActorIterator<APawn> It(World); It; ++It) { Query.AddIgnoredActor(*It); }
	const float Top = Command.AreaCenter.Z + Command.AreaExtent.Z;
	const float Bottom = Command.AreaCenter.Z - Command.AreaExtent.Z;
	const float MinimumNormalZ = FMath::Cos(
		FMath::DegreesToRadians(FMath::Clamp(Command.MaxStaticMeshSlopeAngle, 0.f, 90.f)));
	FVoxelData& Data = VoxelWorld->GetData();
	for (const auto& Column : Columns)
	{
		const FIntPoint XY = Column.Key;
		const FVector Sample = VoxelWorld->LocalToGlobalFloatBP(FVector(XY.X, XY.Y, 0));
		TArray<FHitResult> Hits;
		World->LineTraceMultiByObjectType(Hits, FVector(Sample.X, Sample.Y, Top),
		                                  FVector(Sample.X, Sample.Y, Bottom), Objects, Query);
		const FHitResult* Hit = FindHighestHit(Hits);
		const float MeshZ = Hit ? VoxelWorld->GlobalToLocalFloat(Hit->ImpactPoint).Z : -MAX_flt;
		int32 SolidZ = MIN_int32;
		float VoxelHeight = -MAX_flt;
		{
			const FVoxelIntBox Bounds(FIntVector(XY.X, XY.Y, OutPlan.WriteVoxelMin.Z),
			                          FIntVector(XY.X + 1, XY.Y + 1, OutPlan.WriteVoxelMax.Z + 1));
			FVoxelReadScopeLock Lock(Data, Bounds, FUNCTION_FNAME);
			float Above = Data.GetValue(FIntVector(XY.X, XY.Y, OutPlan.WriteVoxelMax.Z), 0).ToFloat();
			// A solid ceiling has no exposed surface within this area.
			if (Above <= 0.f) { continue; }
			for (int32 Z = OutPlan.WriteVoxelMax.Z - 1; Z >= OutPlan.WriteVoxelMin.Z; --Z)
			{
				const float Below = Data.GetValue(FIntVector(XY.X, XY.Y, Z), 0).ToFloat();
				if (Below <= 0.f)
				{
					SolidZ = Z;
					VoxelHeight = Z + (-Below) / (Above - Below);
					break;
				}
				Above = Below;
			}
		}
		FDRVoxelDepositWrite Write;
		if (Hit && MeshZ >= VoxelHeight)
		{
			// An ineligible roof still occludes the snow below it.
			if (!Command.bDepositOnStaticMeshes || !FMath::IsFinite(MeshZ) ||
				!IsEligibleStaticMeshDepositHit(*Hit, MinimumNormalZ, Command.RequiredStaticMeshSurfaceTag))
			{
				continue;
			}
			Write.Position = FIntVector(XY.X, XY.Y, FMath::FloorToInt(MeshZ) + 1);
			Write.bHasStaticMeshSupport = true;
			Write.StaticMeshSurfaceZ = MeshZ;
			Write.SupportComponent = Hit->GetComponent();
		}
		else if (SolidZ != MIN_int32)
		{
			Write.Position = FIntVector(XY.X, XY.Y, SolidZ + 1);
		}
		else { continue; }
		if (!IsInsideBounds(Write.Position, OutPlan.WriteVoxelMin, OutPlan.WriteVoxelMax) ||
			!Command.ContainsWorldPosition(VoxelWorld->LocalToGlobalFloatBP(FVector(Write.Position)))) { continue; }
		Write.bAllowBelowSupport = Write.Position.Z > OutPlan.WriteVoxelMin.Z &&
			Command.ContainsWorldPosition(
				VoxelWorld->LocalToGlobalFloatBP(FVector(Write.Position - FIntVector(0, 0, 1))));
		Write.AmountScale = Column.Value;
		OutPlan.Writes.Add(Write);
	}
	LevelDepositPlan(VoxelWorld, OutPlan);
	return true;
}

bool FDRVoxelDepositOperations::ApplyDepositPlan(
	AVoxelWorld* VoxelWorld,
	FDRVoxelDepositPlan& Plan,
	FDRVoxelDepositResult& OutResult)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DRDeposit_Apply);
	OutResult = FDRVoxelDepositResult();
	if (Plan.IsEmpty())
	{
		Plan.Reset();
		return true;
	}
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		Plan.Reset();
		return false;
	}

	if (!IsSafeInclusiveVoxelBounds(
		Plan.WriteVoxelMin,
		Plan.WriteVoxelMax))
	{
		Plan.Reset();
		return false;
	}

	const FVoxelMaterial Material = MakeDepositMaterial(Plan.Settings.DepositMaterialIndex);
	OutResult.VoxelWorldName = VoxelWorld->GetFName();
	OutResult.MaterialIndex = Plan.Settings.DepositMaterialIndex;
	FVoxelIntBoxWithValidity ModifiedBounds;
	TSet<FIntVector> WrittenPositions;
	WrittenPositions.Reserve(Plan.Writes.Num() * 2);
	FVoxelData& Data = VoxelWorld->GetData();
	for (const auto& Batch : GroupDataBatches(Plan.Writes, true))
	{
		FVoxelWriteScopeLock Lock(Data, Batch.Value.Bounds.GetBox(), FUNCTION_FNAME);
		for (const int32 Index : Batch.Value.Indices)
		{
			TryApplyWrite(
				Plan,
				Plan.Writes[Index],
				Data,
				Material,
				WrittenPositions,
				ModifiedBounds,
				OutResult.Cells);
		}
	}

	UpdateChangedChunks(VoxelWorld, OutResult.Cells);

	Plan.Reset();
	return true;
}

void FDRVoxelDepositOperations::LevelDepositPlan(AVoxelWorld* VoxelWorld, FDRVoxelDepositPlan& Plan)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() || Plan.IsEmpty() ||
		!FMath::IsFinite(Plan.Settings.LevelingStrength) || Plan.Settings.LevelingStrength <= 0.f)
	{
		return;
	}
	struct FSurface
	{
		float Height;
		const FDRVoxelDepositWrite* Write;
	};
	TMap<FIntPoint, FSurface> Surfaces;
	FVoxelData& Data = VoxelWorld->GetData();
	for (const auto& Batch : GroupDataBatches(Plan.Writes, true))
	{
		FVoxelReadScopeLock Lock(Data, Batch.Value.Bounds.GetBox(), FUNCTION_FNAME);
		for (const int32 Index : Batch.Value.Indices)
		{
			const FDRVoxelDepositWrite& Write = Plan.Writes[Index];
			const float Above = Data.GetValue(Write.Position, 0).ToFloat();
			const float Below = Data.GetValue(Write.Position - FIntVector(0, 0, 1), 0).ToFloat();
			if (Above <= 0.f || (Below > 0.f && !Write.bHasStaticMeshSupport))
			{
				continue;
			}
			const float Height = Below <= 0.f
				                     ? Write.Position.Z - 1.f + (-Below) / (Above - Below)
				                     : Write.StaticMeshSurfaceZ;
			Surfaces.Add(FIntPoint(Write.Position.X, Write.Position.Y), {Height, &Write});
		}
	}

	const float MaxStep = FMath::Tan(FMath::DegreesToRadians(DRDepositMaximumFootprintSlopeDegrees));
	for (FDRVoxelDepositWrite& Write : Plan.Writes)
	{
		const FIntPoint XY(Write.Position.X, Write.Position.Y);
		const FSurface* Center = Surfaces.Find(XY);
		if (!Center)
		{
			continue;
		}
		float TargetSum = 0.f;
		int32 PairCount = 0;
		for (const FIntPoint Axis : {FIntPoint(1, 0), FIntPoint(0, 1)})
		{
			const FSurface* A = Surfaces.Find(XY - Axis);
			const FSurface* B = Surfaces.Find(XY + Axis);
			auto IsConnected = [&](const FSurface* Neighbor)
			{
				return Neighbor &&
					Neighbor->Write->bHasStaticMeshSupport == Write.bHasStaticMeshSupport &&
					Neighbor->Write->SupportComponent == Write.SupportComponent &&
					FMath::Abs(Neighbor->Height - Center->Height) <= MaxStep;
			};
			// 한쪽만 평균내면 평면 경사와 패치 경계까지 기울어집니다.
			if (IsConnected(A) && IsConnected(B))
			{
				TargetSum += (A->Height + B->Height) * 0.5f;
				++PairCount;
			}
		}
		if (PairCount > 0)
		{
			const float Difference = TargetSum / PairCount - Center->Height;
			Write.AmountScale *= FMath::Clamp(
				1.f + FMath::Clamp(Plan.Settings.LevelingStrength, 0.f, 2.f) * Difference, 0.f, 2.f);
		}
	}
}

bool FDRVoxelDepositOperations::ApplyDepositResult(AVoxelWorld* VoxelWorld, const FDRVoxelDepositResult& Result)
{
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}
	if (Result.Cells.IsEmpty())
	{
		return true;
	}
	for (const FDRVoxelDepositCell& Cell : Result.Cells)
	{
		if (Cell.Value < FVoxelValue::MIN_VOXELVALUE || Cell.Value > FVoxelValue::MAX_VOXELVALUE ||
			Cell.Position.GetMin() <= MIN_int32 + 64 || Cell.Position.GetMax() >= MAX_int32 - 64)
		{
			return false;
		}
	}
	const FVoxelMaterial Material = MakeDepositMaterial(Result.MaterialIndex);
	FVoxelData& Data = VoxelWorld->GetData();
	for (const auto& Batch : GroupDataBatches(Result.Cells, false))
	{
		FVoxelWriteScopeLock Lock(Data, Batch.Value.Bounds.GetBox(), FUNCTION_FNAME);
		for (const int32 Index : Batch.Value.Indices)
		{
			const auto& Cell = Result.Cells[Index];
			Data.SetValue(Cell.Position, FVoxelValue::InternalConstructor(Cell.Value));
			if (Cell.bPaintMaterial)
			{
				Data.SetMaterial(Cell.Position, Material);
			}
		}
	}
	UpdateChangedChunks(VoxelWorld, Result.Cells);
	return true;
}

bool FDRVoxelDepositCommand::ContainsWorldPosition(const FVector& Position) const
{
	const FVector Offset = Position - AreaCenter;
	const FVector Extent = AreaExtent.GetAbs();
	if (Offset.ContainsNaN() || Extent.ContainsNaN() || Extent.GetMin() <= 0.0)
	{
		return false;
	}
	if (FMath::Abs(Offset.Z) > Extent.Z)
	{
		return false;
	}
	switch (AreaShape)
	{
	case EDRVoxelDepositAreaShape::Box:
		return FMath::Abs(Offset.X) <= Extent.X && FMath::Abs(Offset.Y) <= Extent.Y;
	case EDRVoxelDepositAreaShape::Sphere:
		return Offset.SizeSquared() <= FMath::Square(Extent.X);
	case EDRVoxelDepositAreaShape::Cylinder:
		return Offset.SizeSquared2D() <= FMath::Square(Extent.X);
	default:
		return false;
	}
}
