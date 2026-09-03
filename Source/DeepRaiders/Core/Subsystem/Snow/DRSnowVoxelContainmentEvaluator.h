#pragma once

#include "CoreMinimal.h"

class AVoxelWorld;
struct FDRSnowSurfaceAddRequest;
struct FDRSnowSurfaceEditResult;
struct FVoxelIntBox;

// Voxel 편집 영역과 캐릭터 Capsule의 교차 여부를 좁힌 뒤,
// 실제 매몰 판정은 각 캐릭터의 UDRVoxelContainmentComponent에 위임한다.
class DEEPRAIDERS_API FDRSnowVoxelContainmentEvaluator
{
public:
	bool EvaluateCharactersInEditedBounds(
		AVoxelWorld& VoxelWorld,
		const FVoxelIntBox& EditedBounds) const;

	// 동기 Add 경로에서는 EditedBounds가 없을 수 있어 요청자를 fallback으로 검사한다.
	void EvaluateAffectedAdd(
		const FDRSnowSurfaceAddRequest& Request,
		const FDRSnowSurfaceEditResult& EditResult) const;
};
