#pragma once

#include "CoreMinimal.h"
#include "DRSnowInteractionComponent.h"
#include "DRSnowRemoveComponent.generated.h"

class AVoxelWorld;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowRemovalSpec
{
	GENERATED_BODY()

	// 제거 brush 반경이다. 무기 정의가 계산한 최종 값을 전달한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (ClampMin = "0.0", Units = "cm"))
	float SnowAbsorbRadius = 100.f;

	// 1회 제거 강도다. ContactBrush에서는 표면 관통 깊이에도 반영된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (ClampMin = "0.0"))
	float SnowAbsorbPower = 1.f;

	// 지속 입력 중 초당 흡수 시도 횟수다. 10이면 0.1초마다 한 번 시도한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove|Timing", meta = (ClampMin = "0.0"))
	float SnowAbsorbSpeed = 10.f;

	// Absorb Tool이 조준 방향으로 검사할 최대 길이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (Units = "cm"))
	float SnowAbsorbRange = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (Units = "cm"))
	FVector SnowAbsorbStartOffset = FVector(75.f, 0.f, 0.f);

	// Adaptive 거리 필드 slab의 최소 절반 깊이다. 기존 데이터 호환을 위해 이름을 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (ClampMin = "1.0", Units = "cm"))
	float SnowAbsorbSweepRadius = 50.f;

	// 한 흡수 틱에 허용할 최대 거리 필드 slab 조회 수다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (ClampMin = "1"))
	int32 SnowAbsorbMaxSweepsPerTick = 32;

	// Absorb Tool의 slab 기반 surface query 사용 여부다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove")
	bool bUseAdaptiveAbsorbQuery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SnowAbsorbInnerRadiusRatio = 0.45f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove")
	EDRSnowRemovalBrushShape RemovalBrushShape = EDRSnowRemovalBrushShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove")
	EDRSnowRemovalMode RemovalMode = EDRSnowRemovalMode::ContactBrush;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDRSnowRemovedSignature,
	const FDRSnowSurfaceRemoveRequest&,
	Request,
	float,
	RemovedAmount);

// 청소기, 제설차처럼 표면의 눈을 흡수하거나 제거하는 Actor에 붙인다.
UCLASS(
	ClassGroup = (Snow),
	BlueprintType,
	Blueprintable,
	meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRSnowRemoveComponent : public UDRSnowInteractionComponent
{
	GENERATED_BODY()

public:
	UDRSnowRemoveComponent();

	UFUNCTION(BlueprintCallable, Category = "Snow|Remove")
	float TryRemoveSnowFromHit(const FHitResult& HitResult, const FDRSnowRemovalSpec& RemovalSpec);

	UFUNCTION(BlueprintCallable, Category = "Snow|Remove")
	float TryRemoveSnowAtLocation(
		FVector WorldLocation,
		FVector SurfaceNormal,
		const FDRSnowRemovalSpec& RemovalSpec);

	// frustum을 slab 단위로 조회한 뒤 전체 후보를 합쳐 흡수한다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Remove")
	float TryRemoveSnowAlongDirection(
		FVector BrushOrigin,
		FVector Direction,
		const FDRSnowRemovalSpec& RemovalSpec);

protected:
	// Hit 위치와 현재 흡수 수치를 조합해 중앙 표면 제거 요청으로 변환한다.
	FDRSnowSurfaceRemoveRequest MakeRemoveRequest(
		FVector WorldLocation,
		FVector SurfaceNormal,
		FVector BrushOrigin,
		const FDRSnowRemovalSpec& RemovalSpec);

	float ExecuteRemoveRequest(
		const FDRSnowSurfaceRemoveRequest& Request,
		AActor* FallbackTarget = nullptr,
		bool bUseAbsorbTool = false);

	// 입력 유지 중 호출자가 반복 요청할 때 서버가 너무 자주 Voxel 편집하지 않도록 제한한다.
	bool CanRemoveNow(const FDRSnowRemovalSpec& RemovalSpec) const;

	UFUNCTION(Server, Reliable)
	void ServerTryRemoveSnowFromHit(const FHitResult& HitResult, const FDRSnowRemovalSpec& RemovalSpec);

	UFUNCTION(Server, Reliable)
	void ServerTryRemoveSnowAtLocation(
		FVector_NetQuantize WorldLocation,
		FVector_NetQuantizeNormal SurfaceNormal,
		const FDRSnowRemovalSpec& RemovalSpec);

	// Voxel collision component를 맞춘 경우 Owner인 AVoxelWorld까지 거슬러 올라간다.
	AVoxelWorld* GetVoxelWorldFromHit(const FHitResult& HitResult) const;

public:
	UPROPERTY(BlueprintAssignable, Category = "Snow|Remove")
	FDRSnowRemovedSignature OnSnowRemoved;

protected:
	float LastRemoveTime = -BIG_NUMBER;
};
