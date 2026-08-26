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
