#pragma once

#include "CoreMinimal.h"
#include "DRProjectile.h"
#include "DRSnowProjectile.generated.h"

class UDRSnowAddComponent;

// 눈총 전용 Projectile. 공통 이동/충돌/GAS Impact 처리는 ADRProjectile이 담당하고,
// 월드 충돌 시 눈 생성만 이 클래스에서 처리한다.
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRSnowProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
	ADRSnowProjectile(const FObjectInitializer& ObjectInitializer);

	virtual float GetConfiguredInitialSpeed() const override;
	virtual float GetConfiguredGravityScale() const override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleWorldImpact(const FHitResult& ImpactResult) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Snow", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDRSnowAddComponent> SnowAddComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow Projectile|Movement", meta = (AllowPrivateAccess = true, ClampMin = "1.0", Units = "cm/s"))
	float InitialSpeed = 3000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Snow Projectile|Movement", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float GravityScale = 1.5f;
};
