#pragma once

#include "CoreMinimal.h"
#include "DRSnowInteractionComponent.h"
#include "DRSnowRemoveComponent.generated.h"

class AVoxelWorld;

UENUM(BlueprintType)
enum class EDRSnowRemovalTraceMode : uint8
{
	LineTrace UMETA(DisplayName = "Line Trace"),
	SphereSweep UMETA(DisplayName = "Sphere Sweep")
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowRemovalSettings
{
	GENERATED_BODY()

	// 청소기 느낌에 따라 정밀한 조준선 또는 넓은 흡입 판정을 선택한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove|Trace")
	EDRSnowRemovalTraceMode TraceMode = EDRSnowRemovalTraceMode::SphereSweep;

	// 흡수 사거리. 업그레이드가 길이를 늘릴 때 이 값을 갱신한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float AbsorbRange = 700.f;

	// SphereSweep 모드에서 표면을 잡아내는 판정 여유 반경이다.
	// 실제 눈 제거 범위는 AbsorbRadius가 담당한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceSweepRadius = 40.f;

	// 실제 Voxel surface edit가 적용되는 반경이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (ClampMin = "0.0", Units = "cm"))
	float AbsorbRadius = 100.f;

	// 1회 흡수 시 surface sculpt에 적용할 강도다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove", meta = (ClampMin = "0.0"))
	float AbsorbStrength = 1.f;

	// 지속 입력 중 흡수를 몇 초마다 한 번 시도할지 정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float AbsorbInterval = 0.1f;
	
	// Voxel 값 방향이 맵/머티리얼 구성과 반대로 느껴질 때 디버그용으로 뒤집는다.
	// 기본 방향이 확정되면 데이터 에셋에서 고정값으로 관리하는 것을 권장한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove")
	bool bInvertSurfaceStrength = false;
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

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Snow|Remove")
	void StartSnowRemoval();

	UFUNCTION(BlueprintCallable, Category = "Snow|Remove")
	void StopSnowRemoval();

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
		FVector SurfaceNormal) const;

	// 지속 흡수 중 카메라/시선 기준으로 흡수 후보 표면을 찾는다.
	bool PerformRemovalTrace(FHitResult& OutHitResult) const;

	// Tick마다 Voxel 편집을 호출하지 않도록 흡수 간격을 제한한다.
	bool CanRemoveNow() const;

	// Voxel collision component를 맞춘 경우 Owner인 AVoxelWorld까지 거슬러 올라간다.
	AVoxelWorld* GetVoxelWorldFromHit(const FHitResult& HitResult) const;

public:
	UPROPERTY(BlueprintAssignable, Category = "Snow|Remove")
	FDRSnowRemovedSignature OnSnowRemoved;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove|Trace")
	bool bTraceComplex = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Remove")
	FDRSnowRemovalSettings RemovalSettings;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Snow|Remove")
	bool bRemovingSnow = false;

	float LastRemoveTime = -BIG_NUMBER;
};
