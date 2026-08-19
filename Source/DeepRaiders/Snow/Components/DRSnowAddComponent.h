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
	// 여러 VoxelWorld가 있는 상황에서는 TryAddSnowAtLocationForTeam을 우선 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	bool TryAddSnowAtLocation(
		FVector WorldLocation,
		FVector SurfaceNormal);

	// 팀과 VoxelWorld를 외부에서 확정해 전달하는 경로다.
	// 투사체, 장판, 디버그처럼 Owner의 팀 추론에 의존하면 안 되는 경우에 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	bool TryAddSnowAtLocationForTeam(
		FVector WorldLocation,
		FVector SurfaceNormal,
		int32 TeamId,
		AVoxelWorld* TargetVoxelWorld);

	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	bool TryAddSnowImpactAtLocationForTeam(
		FVector WorldLocation,
		FVector SurfaceNormal,
		FVector ImpactDirection,
		int32 TeamId,
		AVoxelWorld* TargetVoxelWorld);

	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	void SetAddSettings(float InAddRadius, float InAddAmount);

	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	void SetAddEditTool(EDRSnowVoxelEditTool InEditTool);

	// 카메라 방향으로 trace한 뒤 명시한 팀으로 눈 추가를 시도한다.
	// 실제 팀 material paint는 DRSnowSurfaceSubsystem -> DRVoxelTeamColorLibrary 경로에서 처리한다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Debug")
	bool DebugTryAddSnowFromView(
		float TraceDistance,
		int32 TeamId,
		AVoxelWorld* TargetVoxelWorld);

	UFUNCTION(BlueprintCallable, Category = "Snow|Debug")
	bool DebugTryAddSnowFromViewWithTool(
		float TraceDistance,
		int32 TeamId,
		AVoxelWorld* TargetVoxelWorld,
		EDRSnowVoxelEditTool DebugEditTool);

protected:
	// 위치/노멀/반경/양/기본 interaction context만 채운다.
	// TeamId나 TargetVoxelWorld를 강제로 지정해야 하면 호출자가 Request 생성 후 덮어쓴다.
	FDRSnowSurfaceAddRequest MakeAddRequest(
		FVector WorldLocation,
		FVector SurfaceNormal);

	// 디버그용 시선 trace만 담당한다. 눈 추가/재질 처리는 여기서 하지 않는다.
	bool MakeDebugViewHit(
		float TraceDistance,
		FHitResult& OutHitResult) const;

	// 디버그 trace 결과를 실제 AddSnow 요청으로 바꾸고, 화면 표시용 debug sphere만 그린다.
	bool DebugAddSnowFromHit(
		const FHitResult& HitResult,
		int32 TeamId,
		AVoxelWorld* TargetVoxelWorld,
		EDRSnowVoxelEditTool DebugEditTool);

	// SnowVolume 원본 데이터 갱신 후, 성공한 경우에만 Voxel 표면 표현을 갱신한다.
	bool ExecuteAddSnow(const FDRSnowSurfaceAddRequest& Request);

	UFUNCTION(Server, Reliable)
	void ServerTryAddSnowFromHit(const FHitResult& HitResult);

	UFUNCTION(Server, Reliable)
	void ServerTryAddSnowAtLocation(
		FVector_NetQuantize WorldLocation,
		FVector_NetQuantizeNormal SurfaceNormal);

	UFUNCTION(Server, Reliable)
	void ServerTryAddSnowAtLocationForTeam(
		FVector_NetQuantize WorldLocation,
		FVector_NetQuantizeNormal SurfaceNormal,
		int32 TeamId,
		AVoxelWorld* TargetVoxelWorld);

	UFUNCTION(Server, Reliable)
	void ServerTryAddSnowImpactAtLocationForTeam(
		FVector_NetQuantize WorldLocation,
		FVector_NetQuantizeNormal SurfaceNormal,
		FVector_NetQuantizeNormal ImpactDirection,
		int32 TeamId,
		AVoxelWorld* TargetVoxelWorld);

	UFUNCTION(Server, Reliable)
	void ServerDebugAddSnowAtLocationForTeam(
		FVector_NetQuantize WorldLocation,
		FVector_NetQuantizeNormal SurfaceNormal,
		FVector_NetQuantizeNormal ImpactDirection,
		int32 TeamId,
		AVoxelWorld* TargetVoxelWorld,
		EDRSnowVoxelEditTool DebugEditTool);

	// VoxelWorld actor를 직접 맞거나, VoxelWorld 하위 collision component를 맞은 경우를 모두 처리한다.
	AVoxelWorld* GetVoxelWorldFromHit(const FHitResult& HitResult) const;

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
};
