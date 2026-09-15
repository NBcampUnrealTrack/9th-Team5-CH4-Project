#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Player/GAS/Abilities/DRGA_CharacterSkillBase.h"
#include "DRGA_ProjectileSkillBase.generated.h"

UCLASS(Abstract)
class DEEPRAIDERS_API UDRGA_ProjectileSkillBase : public UDRGA_CharacterSkillBase
{
	GENERATED_BODY()

protected:
	bool ResolveProjectileLaunch(
		const FGameplayAbilityActorInfo* ActorInfo,
		float MaxAimDistance,
		FVector& OutSpawnLocation,
		FVector& OutProjectileDirection) const;
};
