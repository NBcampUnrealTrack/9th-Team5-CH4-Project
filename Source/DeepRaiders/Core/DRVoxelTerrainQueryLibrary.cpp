// DRVoxelTerrainQueryLibrary.cpp

#include "DRVoxelTerrainQueryLibrary.h"

#include "VoxelMaterial.h"
#include "VoxelTools/VoxelDataTools.h"
#include "VoxelWorld.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"

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

bool UDRVoxelTerrainQueryLibrary::AddSnowInBox(
	AVoxelWorld* VoxelWorld,
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	float SampleStep,
	float SnowAmount,
	uint8 SnowMaterialIndex,
	int32& OutModifiedVoxelCount)
{
	OutModifiedVoxelCount = 0;

	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return false;
	}

	if (SampleStep <= 0.f || SnowAmount <= 0.f)
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

	const FIntVector VoxelMin(
		FMath::Min(VoxelA.X, VoxelB.X),
		FMath::Min(VoxelA.Y, VoxelB.Y),
		FMath::Min(VoxelA.Z, VoxelB.Z));

	const FIntVector VoxelMax(
		FMath::Max(VoxelA.X, VoxelB.X),
		FMath::Max(VoxelA.Y, VoxelB.Y),
		FMath::Max(VoxelA.Z, VoxelB.Z));

	const int32 VoxelSampleStep = FMath::Max(
		1,
		FMath::RoundToInt(SampleStep / VoxelWorld->VoxelSize));

	const int32 PatchRadius = FMath::Max(0, VoxelSampleStep / 2);

	FVoxelMaterial SnowMaterial;
	SnowMaterial.SetSingleIndex(SnowMaterialIndex);

	TSet<FIntVector> SnowVoxelPositions;
	SnowVoxelPositions.Reserve(
		((VoxelMax.X - VoxelMin.X) / VoxelSampleStep + 1) *
		((VoxelMax.Y - VoxelMin.Y) / VoxelSampleStep + 1) *
		FMath::Square(PatchRadius * 2 + 1));

	for (int32 X = VoxelMin.X; X <= VoxelMax.X; X += VoxelSampleStep)
	{
		for (int32 Y = VoxelMin.Y; Y <= VoxelMax.Y; Y += VoxelSampleStep)
		{
			for (int32 Z = VoxelMax.Z; Z >= VoxelMin.Z; --Z)
			{
				const FIntVector SurfaceVoxelPosition(X, Y, Z);

				float SurfaceValue = 0.f;
				UVoxelDataTools::GetValue(
					SurfaceValue,
					VoxelWorld,
					SurfaceVoxelPosition);

				if (SurfaceValue > 0.f)
				{
					continue;
				}

				const int32 SnowZ = Z + 1;
				if (SnowZ > VoxelMax.Z)
				{
					break;
				}

				for (int32 PatchX = X - PatchRadius; PatchX <= X + PatchRadius; ++PatchX)
				{
					if (PatchX < VoxelMin.X || PatchX > VoxelMax.X)
					{
						continue;
					}

					for (int32 PatchY = Y - PatchRadius; PatchY <= Y + PatchRadius; ++PatchY)
					{
						if (PatchY < VoxelMin.Y || PatchY > VoxelMax.Y)
						{
							continue;
						}

						SnowVoxelPositions.Add(FIntVector(PatchX, PatchY, SnowZ));
					}
				}

				break;
			}
		}
	}

	for (const FIntVector& SnowVoxelPosition : SnowVoxelPositions)
	{
		float CurrentSnowValue = 0.f;
		UVoxelDataTools::GetValue(
			CurrentSnowValue,
			VoxelWorld,
			SnowVoxelPosition);

		const float NewSnowValue = FMath::Clamp(
			CurrentSnowValue - SnowAmount,
			-1.f,
			1.f);

		UVoxelDataTools::SetValue(
			VoxelWorld,
			SnowVoxelPosition,
			NewSnowValue);

		UVoxelDataTools::SetMaterial(
			VoxelWorld,
			SnowVoxelPosition,
			SnowMaterial);

		OutModifiedVoxelCount++;
	}

	if (OutModifiedVoxelCount > 0)
	{
		const FVoxelIntBox UpdateBounds =
			FVoxelIntBox(VoxelMin, VoxelMax).Extend(2);

		UVoxelBlueprintLibrary::UpdateBounds(
			VoxelWorld,
			UpdateBounds);
	}

	return true;
}
