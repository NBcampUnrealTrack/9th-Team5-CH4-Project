#include "DRGA_TestAddSnow.h"

#include "GameplayEffect.h"

UDRGA_TestAddSnow::UDRGA_TestAddSnow()
{
	InstancingPolicy =
		EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy =
		EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UDRGA_TestAddSnow::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		TriggerEventData);

	AActor* AvatarActor = GetAvatarActorFromActorInfo();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[GAS][GA_TestAddSnow] Activate "
			"Avatar=%s Authority=%d"),
		*GetNameSafe(AvatarActor),
		IsValid(AvatarActor)
			? AvatarActor->HasAuthority()
			: false);

	if (IsValid(AddSnowEffectClass))
	{
		const UGameplayEffect* Effect =
			AddSnowEffectClass->GetDefaultObject<UGameplayEffect>();

		ApplyGameplayEffectToOwner(
			Handle,
			ActorInfo,
			ActivationInfo,
			Effect,
			1.f,
			1);
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false);
}