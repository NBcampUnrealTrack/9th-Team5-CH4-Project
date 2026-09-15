#pragma once

#include "CoreMinimal.h"
#include "DRSnowInteractionComponent.h"
#include "DRSnowAddComponent.generated.h"

class AVoxelWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDRSnowAddedSignature,
	const FDRSnowSurfaceAddRequest&,
	Request,
	bool,
	bHandled);

// 투사체, 폭발, 스킬 장판처럼 표면에 눈을 쌓는 Actor에 붙인다.
UCLASS(
	ClassGroup = (Snow),
	BlueprintType,
	Blueprintable,
	meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRSnowAddComponent : public UDRSnowInteractionComponent
{
	GENERATED_BODY()

public:
	// Hit actor/component에서 VoxelWorld를 추적해 눈 추가를 시도한다.
	// VoxelWorld가 아닌 상호작용 대상은 DRSnowInteractableInterface fallback으로 처리한다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	bool TryAddSnowFromHit(const FHitResult& HitResult);

	// 호출자의 Owner/PlayerState에서 팀을 해석하고, 대상 VoxelWorld는 subsystem이 찾는다.
	// 명시적인 팀/월드/방향이 필요하면 TryAddSnow request API를 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	bool TryAddSnowAtLocation(FVector WorldLocation, FVector SurfaceNormal);

	// 팀, VoxelWorld, ImpactDirection을 호출자가 직접 채운 상세 요청 경로다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	bool TryAddSnow(const FDRSnowSurfaceAddRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	void SetAddSettings(float InAddRadius, float InAddAmount);

	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	void SetAddEditTool(EDRSnowVoxelEditTool InEditTool);

	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	void SetAllowVirtualSurfaceFallback(bool bInAllowVirtualSurfaceFallback);

protected:
	// 위치/노멀/반경/양/기본 interaction context만 채운다.
	// TeamId나 TargetVoxelWorld를 강제로 지정해야 하면 호출자가 Request 생성 후 덮어쓴다.
	FDRSnowSurfaceAddRequest MakeAddRequest(FVector WorldLocation, FVector SurfaceNormal);

	// Voxel add, 상호작용 fallback, operation 등록, 이벤트 broadcast를 공통 처리한다.
	bool ExecuteAddRequest(const FDRSnowSurfaceAddRequest& Request, AActor* FallbackTarget = nullptr);

	UFUNCTION(Server, Reliable)
	void ServerTryAddSnowFromHit(const FHitResult& HitResult);

	UFUNCTION(Server, Reliable)
	void ServerTryAddSnow(const FDRSnowSurfaceAddRequest& Request);

	// VoxelWorld actor를 직접 맞거나, VoxelWorld 하위 collision component를 맞은 경우를 모두 처리한다.
	AVoxelWorld* GetVoxelWorldFromHit(const FHitResult& HitResult) const;

	AVoxelWorld* ResolveFallbackVoxelWorld() const;

public:
	UPROPERTY(BlueprintAssignable, Category = "Snow|Add")
	FDRSnowAddedSignature OnSnowAdded;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Add", meta = (ClampMin = "0.0", Units = "cm"))
	float AddRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Add", meta = (ClampMin = "0.0"))
	float AddAmount = 1.f;

	// Voxel 표면을 어떤 방식으로 올릴지 선택한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Add")
	EDRSnowVoxelEditTool AddEditTool = EDRSnowVoxelEditTool::SurfaceTool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Add")
	bool bAllowVirtualSurfaceFallback = false;
};
