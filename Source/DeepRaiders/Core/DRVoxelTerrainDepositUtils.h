#pragma once

#include "CoreMinimal.h"

namespace DRVoxelTerrain
{
	constexpr int32 QuantizedValueMax = 32767;

	struct FInclusiveVoxelBoxDimensions
	{
		int32 SizeX = 0;
		int32 SizeXY = 0;
		int32 TotalVoxelCount = 0;
	};

	inline bool TryGetInclusiveVoxelBoxDimensions(
		const FIntVector& VoxelMin,
		const FIntVector& VoxelMax,
		FInclusiveVoxelBoxDimensions& OutDimensions,
		bool bRequireExpandableBounds = false)
	{
		OutDimensions = FInclusiveVoxelBoxDimensions();
		if (bRequireExpandableBounds &&
			(VoxelMin.X == MIN_int32 || VoxelMin.Y == MIN_int32 || VoxelMin.Z == MIN_int32 ||
				VoxelMax.X == MAX_int32 || VoxelMax.Y == MAX_int32 || VoxelMax.Z == MAX_int32))
		{
			return false;
		}

		const int64 SizeX = static_cast<int64>(VoxelMax.X) - VoxelMin.X + 1;
		const int64 SizeY = static_cast<int64>(VoxelMax.Y) - VoxelMin.Y + 1;
		const int64 SizeZ = static_cast<int64>(VoxelMax.Z) - VoxelMin.Z + 1;
		if (SizeX <= 0 || SizeY <= 0 || SizeZ <= 0 ||
			SizeX > MAX_int32 || SizeY > MAX_int32 || SizeZ > MAX_int32 ||
			SizeX > MAX_int32 / SizeY || SizeX * SizeY > MAX_int32 / SizeZ)
		{
			return false;
		}

		OutDimensions.SizeX = static_cast<int32>(SizeX);
		OutDimensions.SizeXY = static_cast<int32>(SizeX * SizeY);
		OutDimensions.TotalVoxelCount = static_cast<int32>(SizeX * SizeY * SizeZ);
		return true;
	}

	inline int32 GetInclusiveVoxelLocalIndex(
		const FIntVector& Position,
		const FIntVector& VoxelMin,
		const FIntVector& VoxelMax)
	{
		FInclusiveVoxelBoxDimensions Dimensions;
		const bool bHasValidDimensions = TryGetInclusiveVoxelBoxDimensions(
			VoxelMin,
			VoxelMax,
			Dimensions);
		check(bHasValidDimensions);
		const int64 LocalIndex =
			(Position.X - VoxelMin.X) +
			static_cast<int64>(Position.Y - VoxelMin.Y) * Dimensions.SizeX +
			static_cast<int64>(Position.Z - VoxelMin.Z) * Dimensions.SizeXY;
		check(LocalIndex >= 0 && LocalIndex < Dimensions.TotalVoxelCount);
		return static_cast<int32>(LocalIndex);
	}

	inline FIntVector GetInclusiveVoxelPosition(
		int32 LocalIndex,
		const FIntVector& VoxelMin,
		const FInclusiveVoxelBoxDimensions& Dimensions)
	{
		check(LocalIndex >= 0 && LocalIndex < Dimensions.TotalVoxelCount);
		const int32 LocalZ = LocalIndex / Dimensions.SizeXY;
		const int32 Remainder = LocalIndex % Dimensions.SizeXY;
		const int32 LocalY = Remainder / Dimensions.SizeX;
		const int32 LocalX = Remainder % Dimensions.SizeX;
		return VoxelMin + FIntVector(LocalX, LocalY, LocalZ);
	}

	template<typename ElementType>
	void ShuffleArray(TArray<ElementType>& Values, FRandomStream& RandomStream)
	{
		for (int32 Index = Values.Num() - 1; Index > 0; --Index)
		{
			Values.Swap(Index, RandomStream.RandRange(0, Index));
		}
	}

	inline bool EvaluateFootprintOffset(
		int32 OffsetX,
		int32 OffsetY,
		int32 Radius,
		float EdgeStrength,
		float MaximumSlopeTangent,
		float& OutAmountScale,
		float& OutAllowedHeightDelta)
	{
		const float RadiusAsFloat = static_cast<float>(FMath::Max(1, Radius));
		const float Distance = FMath::Sqrt(
			static_cast<float>(OffsetX * OffsetX + OffsetY * OffsetY));
		if (Radius > 0 && Distance > RadiusAsFloat + 0.5f)
		{
			return false;
		}

		const float DistanceAlpha = Radius > 0
			? FMath::Clamp(Distance / RadiusAsFloat, 0.f, 1.f)
			: 0.f;
		OutAmountScale = FMath::Lerp(1.f, EdgeStrength, DistanceAlpha);
		OutAllowedHeightDelta = FMath::Max(
			1.f,
			Distance * MaximumSlopeTangent + 0.5f);
		return true;
	}
}
