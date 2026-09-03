#pragma once

#include "CoreMinimal.h"

class AVoxelWorld;
struct FDRSnowSurfaceEditResult;
struct FVoxelIntBox;

// Voxel 편집 영역과 캐릭터 Capsule의 교차 여부를 좁힌 뒤,
// 실제 매몰 판정은 각 캐릭터의 UDRVoxelContainmentComponent에 위임한다.
class DEEPRAIDERS_API FDRSnowVoxelContainmentEvaluator
{
public:
	void EvaluateCharactersInEditedBounds(
		AVoxelWorld& VoxelWorld,
		const FVoxelIntBox& EditedBounds) const;

	void EvaluateSurfaceEdit(const FDRSnowSurfaceEditResult& EditResult) const;
};
