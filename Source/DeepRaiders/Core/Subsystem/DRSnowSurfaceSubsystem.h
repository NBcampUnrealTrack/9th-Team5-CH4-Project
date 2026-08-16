#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DRSnowSurfaceSubsystem.generated.h"

class AVoxelWorld;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FDRSnowRemovedFromSurfaceDelegate,
	const FDRSnowSurfaceRemoveRequest&,
	float);

UCLASS()
class DEEPRAIDERS_API UDRSnowSurfaceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// 눈 흡수의 중앙 진입점이다.
	// 외부 컴포넌트는 Voxel Plugin API를 직접 호출하지 않고 이 함수만 사용한다.
	float RemoveSnowAtArea(const FDRSnowSurfaceRemoveRequest& Request);

	FDRSnowRemovedFromSurfaceDelegate OnSnowRemovedFromSurface;

private:
	// 요청에 명시된 VoxelWorld가 없으면 현재 월드의 첫 AVoxelWorld를 임시 대상으로 사용한다.
	// 여러 VoxelWorld를 운용하게 되면 hit 기반 resolve를 우선하도록 확장해야 한다.
	AVoxelWorld* ResolveVoxelWorld(const FDRSnowSurfaceRemoveRequest& Request) const;
};
