#include "DRGA_TestSkill.h"

void UDRGA_TestSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo == nullptr
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[CharacterSkill][Test] Activated. Avatar=%s Authority=%d LocallyControlled=%d"),
		*GetNameSafe(ActorInfo->AvatarActor.Get()),
		ActorInfo->IsNetAuthority(),
		ActorInfo->IsLocallyControlled());

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
