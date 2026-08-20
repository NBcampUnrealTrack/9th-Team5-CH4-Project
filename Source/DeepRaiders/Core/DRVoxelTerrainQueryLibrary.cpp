// DRVoxelTerrainQueryLibrary.cpp

#include "DRVoxelTerrainQueryLibrary.h"

#include "VoxelMaterial.h"
#include "VoxelTools/VoxelDataTools.h"
#include "VoxelWorld.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"

namespace
{
	bool IsDepositRequestFinished(const FDRVoxelDepositInBoxRequest& Request)
	{
		return Request.NextColumnIndex >= Request.PendingColumns.Num();
	}

	void ShuffleColumns(TArray<FIntPoint>& Columns, FRandomStream& RandomStream)
	{
		for (int32 Index = Columns.Num() - 1; Index > 0; --Index)
		{
			const int32 SwapIndex = RandomStream.RandRange(0, Index);
			Columns.Swap(Index, SwapIndex);
		}
	}

	void ExpandModifiedBounds(
		const FIntVector& Position,
		bool& bHasModifiedBounds,
		FIntVector& ModifiedMin,
		FIntVector& ModifiedMax)
	{
		if (!bHasModifiedBounds)
		{
			ModifiedMin = Position;
			ModifiedMax = Position;
			bHasModifiedBounds = true;
			return;
		}

		ModifiedMin.X = FMath::Min(ModifiedMin.X, Position.X);
		ModifiedMin.Y = FMath::Min(ModifiedMin.Y, Position.Y);
		ModifiedMin.Z = FMath::Min(ModifiedMin.Z, Position.Z);

		ModifiedMax.X = FMath::Max(ModifiedMax.X, Position.X);
		ModifiedMax.Y = FMath::Max(ModifiedMax.Y, Position.Y);
		ModifiedMax.Z = FMath::Max(ModifiedMax.Z, Position.Z);
	}

	int32 FindSurfaceZ(
		AVoxelWorld* VoxelWorld,
		int32 X,
		int32 Y,
		int32 MinZ,
		int32 MaxZ)
	{
		float AboveValue = 0.f;
		UVoxelDataTools::GetValue(
			AboveValue,
			VoxelWorld,
			FIntVector(X, Y, MaxZ));

		for (int32 Z = MaxZ - 1; Z >= MinZ; --Z)
		{
			float CurrentValue = 0.f;
			UVoxelDataTools::GetValue(
				CurrentValue,
				VoxelWorld,
				FIntVector(X, Y, Z));

			const bool bCurrentIsSolid = CurrentValue <= 0.f;
			const bool bAboveIsEmpty = AboveValue > 0.f;

			if (bCurrentIsSolid && bAboveIsEmpty)
			{
				return Z;
			}

			AboveValue = CurrentValue;
		}

		return MIN_int32;
	}

	bool WriteDepositVoxel(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		const FVoxelMaterial& DepositMaterial,
		const FIntVector& DepositVoxelPosition,
		bool& bHasModifiedBounds,
		FIntVector& ModifiedMin,
		FIntVector& ModifiedMax,
		int32& OutModifiedVoxelCount)
	{
		if (DepositVoxelPosition.X < Request.VoxelMin.X || DepositVoxelPosition.X > Request.VoxelMax.X ||
			DepositVoxelPosition.Y < Request.VoxelMin.Y || DepositVoxelPosition.Y > Request.VoxelMax.Y ||
			DepositVoxelPosition.Z < Request.VoxelMin.Z || DepositVoxelPosition.Z > Request.VoxelMax.Z)
		{
			return false;
		}

		if (Request.WrittenVoxelPositions.Contains(DepositVoxelPosition))
		{
			return false;
		}

		Request.WrittenVoxelPositions.Add(DepositVoxelPosition);

		float CurrentValue = 0.f;
		UVoxelDataTools::GetValue(
			CurrentValue,
			VoxelWorld,
			DepositVoxelPosition);

		const float NewValue = FMath::Clamp(
			CurrentValue - Request.DepositAmount,
			-1.f,
			1.f);

		UVoxelDataTools::SetValue(
			VoxelWorld,
			DepositVoxelPosition,
			NewValue);

		UVoxelDataTools::SetMaterial(
			VoxelWorld,
			DepositVoxelPosition,
			DepositMaterial);

		ExpandModifiedBounds(
			DepositVoxelPosition,
			bHasModifiedBounds,
			ModifiedMin,
			ModifiedMax);

		OutModifiedVoxelCount++;
		return true;
	}

	void ProcessDepositColumn(
		FDRVoxelDepositInBoxRequest& Request,
		AVoxelWorld* VoxelWorld,
		const FVoxelMaterial& DepositMaterial,
		const FIntPoint& Column,
		bool& bHasModifiedBounds,
		FIntVector& ModifiedMin,
		FIntVector& ModifiedMax,
		int32& OutModifiedVoxelCount)
	{
		const int32 SurfaceZ = FindSurfaceZ(
			VoxelWorld,
			Column.X,
			Column.Y,
			Request.VoxelMin.Z,
			Request.VoxelMax.Z);

		if (SurfaceZ == MIN_int32)
		{
			return;
		}

		const int32 Radius = FMath::Max(
			FMath::Max(0, Request.SmoothRadius),
			Request.VoxelSampleStep / 2);

		for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
		{
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				const int32 TargetX = Column.X + OffsetX;
				const int32 TargetY = Column.Y + OffsetY;

				if (TargetX < Request.VoxelMin.X || TargetX > Request.VoxelMax.X ||
					TargetY < Request.VoxelMin.Y || TargetY > Request.VoxelMax.Y)
				{
					continue;
				}

				const int32 NeighborSurfaceZ = FindSurfaceZ(
					VoxelWorld,
					TargetX,
					TargetY,
					Request.VoxelMin.Z,
					Request.VoxelMax.Z);

				if (NeighborSurfaceZ == MIN_int32)
				{
					continue;
				}

				const int32 HeightDiff = SurfaceZ - NeighborSurfaceZ;
				const bool bNeedsSmoothing = HeightDiff > Request.MaxHeightStep;

				const int32 DepositZ = bNeedsSmoothing
					? NeighborSurfaceZ + 1
					: SurfaceZ + 1;

				WriteDepositVoxel(
					Request,
					VoxelWorld,
					DepositMaterial,
					FIntVector(TargetX, TargetY, DepositZ),
					bHasModifiedBounds,
					ModifiedMin,
					ModifiedMax,
					OutModifiedVoxelCount);
			}
		}
	}

	bool ProcessDepositInBoxRequestTickInternal(
		FDRVoxelDepositInBoxRequest& Request,
		int32 MaxColumnsToProcess,
		int32& OutModifiedVoxelCount,
		int32& OutProcessedColumnCount,
		bool& bOutFinished)
	{
		OutModifiedVoxelCount = 0;
		OutProcessedColumnCount = 0;
		bOutFinished = false;

		AVoxelWorld* VoxelWorld = Request.VoxelWorld.Get();
		if (!Request.bIsValid || !IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
		{
			bOutFinished = true;
			return false;
		}

		if (Request.VoxelSampleStep <= 0 || Request.DepositAmount <= 0.f)
		{
			bOutFinished = true;
			return false;
		}

		FVoxelMaterial DepositMaterial;
		DepositMaterial.SetSingleIndex(Request.DepositMaterialIndex);

		bool bHasModifiedBounds = false;
		FIntVector ModifiedMin = FIntVector::ZeroValue;
		FIntVector ModifiedMax = FIntVector::ZeroValue;

		const int32 ColumnsToProcess = FMath::Max(1, MaxColumnsToProcess);

		while (OutProcessedColumnCount < ColumnsToProcess && !IsDepositRequestFinished(Request))
		{
			const FIntPoint Column = Request.PendingColumns[Request.NextColumnIndex];
			Request.NextColumnIndex++;

			ProcessDepositColumn(
				Request,
				VoxelWorld,
				DepositMaterial,
				Column,
				bHasModifiedBounds,
				ModifiedMin,
				ModifiedMax,
				OutModifiedVoxelCount);

			OutProcessedColumnCount++;
		}

		if (bHasModifiedBounds)
		{
			const FVoxelIntBox UpdateBounds(ModifiedMin, ModifiedMax);

			UVoxelBlueprintLibrary::UpdateBounds(
				VoxelWorld,
				UpdateBounds.Extend(1));
		}

		bOutFinished = IsDepositRequestFinished(Request);
		return true;
	}
}

bool UDRVoxelTerrainQueryLibrary::GetMaterialCountsInBox(
	AVoxelWorld* VoxelWorld,
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	float SampleStep,
	const TArray<uint8>& TargetMaterialIndices,
	TMap<uint8, int32>& OutMaterialCounts,
	int32& OutTotalCount)
{
	OutMaterialCounts.Reset();
	OutTotalCount = 0;

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	if (SampleStep <= 0.f)
	{
		return false;
	}

	const FVector AbsExtent(
		FMath::Abs(BoxExtent.X),
		FMath::Abs(BoxExtent.Y),
		FMath::Abs(BoxExtent.Z));

	if (AbsExtent.IsNearlyZero())
	{
		return false;
	}

	TSet<uint8> TargetMaterialSet;
	for (const uint8 MaterialIndex : TargetMaterialIndices)
	{
		TargetMaterialSet.Add(MaterialIndex);
	}

	const bool bUseMaterialFilter = TargetMaterialSet.Num() > 0;

	const FVector Min = BoxCenter - AbsExtent;
	const FVector Max = BoxCenter + AbsExtent;

	for (float X = Min.X; X <= Max.X; X += SampleStep)
	{
		for (float Y = Min.Y; Y <= Max.Y; Y += SampleStep)
		{
			for (float Z = Min.Z; Z <= Max.Z; Z += SampleStep)
			{
				const FVector SampleWorldPosition(X, Y, Z);
				const FIntVector SampleVoxelPosition = VoxelWorld->GlobalToLocal(SampleWorldPosition);

				float Value = 0.f;
				UVoxelDataTools::GetValue(
					Value,
					VoxelWorld,
					SampleVoxelPosition);

				// Voxel Plugin 1.2 기준으로 보통 Value <= 0 이 solid.
				if (Value > 0.f)
				{
					continue;
				}

				FVoxelMaterial Material;
				UVoxelDataTools::GetMaterial(
					Material,
					VoxelWorld,
					SampleVoxelPosition);

				const uint8 MaterialIndex = Material.GetSingleIndex();

				if (bUseMaterialFilter && !TargetMaterialSet.Contains(MaterialIndex))
				{
					continue;
				}

				OutMaterialCounts.FindOrAdd(MaterialIndex)++;
				OutTotalCount++;
			}
		}
	}

	return true;
}

bool UDRVoxelTerrainQueryLibrary::IsVoxelUpdateInBox(
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	const FVector& Location,
	float Radius)
{
	if (Radius <= 0.f)
	{
		return false;
	}

	const FVector AbsExtent(
		FMath::Abs(BoxExtent.X),
		FMath::Abs(BoxExtent.Y),
		FMath::Abs(BoxExtent.Z));

	if (AbsExtent.IsNearlyZero())
	{
		return false;
	}

	const FBox QueryBox(BoxCenter - AbsExtent, BoxCenter + AbsExtent);
	const FVector ClosestPoint = QueryBox.GetClosestPointTo(Location);

	return FVector::DistSquared(ClosestPoint, Location) <= FMath::Square(Radius);
}

bool UDRVoxelTerrainQueryLibrary::MakeDepositInBoxRequest(
	AVoxelWorld* VoxelWorld,
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	float SampleStep,
	float DepositAmount,
	uint8 DepositMaterialIndex,
	int32 SmoothRadius,
	int32 MaxHeightStep,
	int32 RandomSeed,
	FDRVoxelDepositInBoxRequest& OutRequest)
{
	OutRequest = FDRVoxelDepositInBoxRequest();

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	if (SampleStep <= 0.f || DepositAmount <= 0.f || VoxelWorld->VoxelSize <= 0.f)
	{
		return false;
	}

	const FVector AbsExtent(
		FMath::Abs(BoxExtent.X),
		FMath::Abs(BoxExtent.Y),
		FMath::Abs(BoxExtent.Z));

	if (AbsExtent.IsNearlyZero())
	{
		return false;
	}

	const FVector WorldMin = BoxCenter - AbsExtent;
	const FVector WorldMax = BoxCenter + AbsExtent;

	const FIntVector VoxelA = VoxelWorld->GlobalToLocal(WorldMin);
	const FIntVector VoxelB = VoxelWorld->GlobalToLocal(WorldMax);

	OutRequest.VoxelWorld = VoxelWorld;

	OutRequest.VoxelMin = FIntVector(
		FMath::Min(VoxelA.X, VoxelB.X),
		FMath::Min(VoxelA.Y, VoxelB.Y),
		FMath::Min(VoxelA.Z, VoxelB.Z));

	OutRequest.VoxelMax = FIntVector(
		FMath::Max(VoxelA.X, VoxelB.X),
		FMath::Max(VoxelA.Y, VoxelB.Y),
		FMath::Max(VoxelA.Z, VoxelB.Z));

	if (OutRequest.VoxelMax.Z - OutRequest.VoxelMin.Z < 1)
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}

	OutRequest.VoxelSampleStep = FMath::Max(
		1,
		FMath::RoundToInt(SampleStep / VoxelWorld->VoxelSize));

	OutRequest.SmoothRadius = FMath::Max(0, SmoothRadius);
	OutRequest.MaxHeightStep = FMath::Max(0, MaxHeightStep);
	OutRequest.DepositAmount = DepositAmount;
	OutRequest.DepositMaterialIndex = DepositMaterialIndex;

	FRandomStream RandomStream(RandomSeed);
	const int32 HalfStep = FMath::Max(0, OutRequest.VoxelSampleStep / 2);

	for (int32 X = OutRequest.VoxelMin.X; X <= OutRequest.VoxelMax.X; X += OutRequest.VoxelSampleStep)
	{
		for (int32 Y = OutRequest.VoxelMin.Y; Y <= OutRequest.VoxelMax.Y; Y += OutRequest.VoxelSampleStep)
		{
			const int32 JitteredX = FMath::Clamp(
				X + RandomStream.RandRange(-HalfStep, HalfStep),
				OutRequest.VoxelMin.X,
				OutRequest.VoxelMax.X);

			const int32 JitteredY = FMath::Clamp(
				Y + RandomStream.RandRange(-HalfStep, HalfStep),
				OutRequest.VoxelMin.Y,
				OutRequest.VoxelMax.Y);

			OutRequest.PendingColumns.Add(FIntPoint(JitteredX, JitteredY));
		}
	}

	if (OutRequest.PendingColumns.Num() == 0)
	{
		OutRequest = FDRVoxelDepositInBoxRequest();
		return false;
	}

	ShuffleColumns(OutRequest.PendingColumns, RandomStream);
	OutRequest.NextColumnIndex = 0;
	OutRequest.bIsValid = true;

	return true;
}

bool UDRVoxelTerrainQueryLibrary::ProcessDepositInBoxRequestsTick(
	TArray<FDRVoxelDepositInBoxRequest>& Requests,
	int32 MaxColumnsToProcess,
	int32& OutModifiedVoxelCount,
	int32& OutRemainingRequestCount)
{
	OutModifiedVoxelCount = 0;
	OutRemainingRequestCount = 0;

	if (Requests.Num() == 0)
	{
		return false;
	}

	int32 RemainingColumnsToProcess = FMath::Max(1, MaxColumnsToProcess);
	int32 RequestIndex = 0;

	while (RequestIndex < Requests.Num() && RemainingColumnsToProcess > 0)
	{
		FDRVoxelDepositInBoxRequest& Request = Requests[RequestIndex];

		int32 ModifiedByRequest = 0;
		int32 ProcessedColumns = 0;
		bool bFinished = false;

		const bool bProcessed = ProcessDepositInBoxRequestTickInternal(
			Request,
			RemainingColumnsToProcess,
			ModifiedByRequest,
			ProcessedColumns,
			bFinished);

		OutModifiedVoxelCount += ModifiedByRequest;
		RemainingColumnsToProcess -= ProcessedColumns;

		if (!bProcessed || bFinished)
		{
			Requests.RemoveAt(RequestIndex, 1, false);
			continue;
		}

		RequestIndex++;
	}

	OutRemainingRequestCount = Requests.Num();
	return OutRemainingRequestCount > 0;
}