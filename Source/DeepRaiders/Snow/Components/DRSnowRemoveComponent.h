#pragma once

#include "CoreMinimal.h"
#include "DRSnowInteractionComponent.h"
#include "DRSnowRemoveComponent.generated.h"

class AVoxelWorld;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowRemovalSettings
{
	GENERATED_BODY()

	// 제거 brush 반경이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (ClampMin = "0.0", Units = "cm"))
	float AbsorbRadius = 100.f;

	// 1회 제거 강도다. ContactBrush에서는 표면 관통 깊이에도 반영된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (ClampMin = "0.0"))
	float AbsorbStrength = 1.f;

	// 지속 입력 중 흡수를 몇 초마다 한 번 시도할지 정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float AbsorbInterval = 0.1f;
	
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

	// 장비와 업그레이드가 계산한 최종 흡수 수치를 컴포넌트에 적용한다.
	// Character는 수치를 직접 들지 않고 장착 장비/업그레이드 결과만 전달한다.
	UFUNCTION(BlueprintCallable, Category = "Snow|Remove")
	void ApplyRemovalSettings(const FDRSnowRemovalSettings& NewSettings);

	UFUNCTION(BlueprintPure, Category = "Snow|Remove")
	FDRSnowRemovalSettings GetRemovalSettings() const { return RemovalSettings; }

	UFUNCTION(BlueprintCallable, Category = "Snow|Remove")
	float TryRemoveSnowFromHit(const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = "Snow|Remove")
	float TryRemoveSnowAtLocation(
		FVector WorldLocation,
		FVector SurfaceNormal);

protected:
	// Hit 위치와 현재 업그레이드 수치를 조합해 중앙 표면 제거 요청으로 변환한다.
	FDRSnowSurfaceRemoveRequest MakeRemoveRequest(
		FVector WorldLocation,
		FVector SurfaceNormal,
		FVector BrushOrigin);

	float ExecuteRemoveRequest(const FDRSnowSurfaceRemoveRequest& Request, AActor* FallbackTarget = nullptr);

	// 입력 유지 중 호출자가 반복 요청할 때 서버가 너무 자주 Voxel 편집하지 않도록 제한한다.
	bool CanRemoveNow() const;

	UFUNCTION(Server, Reliable)
	void ServerTryRemoveSnowFromHit(const FHitResult& HitResult);

	UFUNCTION(Server, Reliable)
	void ServerTryRemoveSnowAtLocation(FVector_NetQuantize WorldLocation, FVector_NetQuantizeNormal SurfaceNormal);

	// Voxel collision component를 맞춘 경우 Owner인 AVoxelWorld까지 거슬러 올라간다.
	AVoxelWorld* GetVoxelWorldFromHit(const FHitResult& HitResult) const;

public:
	UPROPERTY(BlueprintAssignable, Category = "Snow|Remove")
	FDRSnowRemovedSignature OnSnowRemoved;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove")
	FDRSnowRemovalSettings RemovalSettings;

	float LastRemoveTime = -BIG_NUMBER;

};
