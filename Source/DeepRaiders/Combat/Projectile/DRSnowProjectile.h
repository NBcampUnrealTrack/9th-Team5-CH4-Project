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
	ADRSnowProjectile();

protected:
	virtual void HandleWorldImpact(const FHitResult& ImpactResult) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Snow", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDRSnowAddComponent> SnowAddComponent;
};
