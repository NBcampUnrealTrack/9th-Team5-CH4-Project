#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DRGE_SkillCooldown.generated.h"

UCLASS(Abstract)
class DEEPRAIDERS_API UDRGE_SkillCooldown : public UGameplayEffect
{
	GENERATED_BODY()

protected:
	UDRGE_SkillCooldown(const FObjectInitializer& ObjectInitializer);
	void ConfigureCooldown(
		const FObjectInitializer& ObjectInitializer,
		FGameplayTag CooldownTag);
};

/**
 * 사용한 차지마다 순차 만료 시간을 예약하는 차지형 쿨다운이다.
 * 스태킹 정책에 의존하지 않으며, Ability가 기존 큐 끝에 새 충전을 붙인다.
 */
UCLASS()
class DEEPRAIDERS_API UDRGE_SkillChargeCooldown : public UDRGE_SkillCooldown
{
	GENERATED_BODY()

public:
	UDRGE_SkillChargeCooldown(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class DEEPRAIDERS_API UDRGE_SkillOneCooldown : public UDRGE_SkillCooldown
{
	GENERATED_BODY()

public:
	UDRGE_SkillOneCooldown(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class DEEPRAIDERS_API UDRGE_SkillTwoCooldown : public UDRGE_SkillCooldown
{
	GENERATED_BODY()

public:
	UDRGE_SkillTwoCooldown(const FObjectInitializer& ObjectInitializer);
};
