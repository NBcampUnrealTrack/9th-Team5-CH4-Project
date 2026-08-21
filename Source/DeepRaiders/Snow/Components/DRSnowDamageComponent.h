#pragma once

#include "CoreMinimal.h"
#include "DRSnowInteractionComponent.h"
#include "DRSnowDamageComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDRSnowDamagedSignature,
	const FDRSnowDamageRequest&,
	Request,
	bool,
	bHandled);

// 눈 투사체나 충돌체처럼 캐릭터/대상에게 눈 피해를 전달하는 Actor에 붙인다.
UCLASS(
	ClassGroup = (Snow),
	BlueprintType,
	Blueprintable,
	meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRSnowDamageComponent : public UDRSnowInteractionComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Snow|Damage")
	bool TryApplySnowDamageFromHit(const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = "Snow|Damage")
	bool TryApplySnowDamageToActor(
		AActor* TargetActor,
		FVector HitLocation,
		FVector HitNormal);

protected:
	FDRSnowDamageRequest MakeDamageRequest(
		FVector HitLocation,
		FVector HitNormal) const;

public:
	UPROPERTY(BlueprintAssignable, Category = "Snow|Damage")
	FDRSnowDamagedSignature OnSnowDamaged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Damage", meta = (ClampMin = "0.0"))
	float DamageAmount = 1.f;
};
