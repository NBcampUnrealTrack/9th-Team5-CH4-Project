#include "DRGA_TestToggleFrozen.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

UDRGA_TestToggleFrozen::UDRGA_TestToggleFrozen()
{
	InstancingPolicy =
		EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy =
		EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UDRGA_TestToggleFrozen::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, 
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	if (!IsValid(ASC))
	{
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			true,
			false);

		return;
	}

	const bool bWasFrozen = ASC->HasMatchingGameplayTag(DRGameplayTags::State_Frozen);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GAS][Frozen] Before=%d"),
		bWasFrozen);
	
	if (!bWasFrozen)
	{
		if (IsValid(FrozenEffectClass))
		{
			const UGameplayEffect* Effect = FrozenEffectClass->GetDefaultObject<UGameplayEffect>();

			ApplyGameplayEffectToOwner(
				Handle,
				ActorInfo,
				ActivationInfo,
				Effect,
				1.f,
				1);
		}
	}
	else
	{
		FGameplayTagContainer FrozenTags;
		FrozenTags.AddTag(DRGameplayTags::State_Frozen);

		ASC->RemoveActiveEffectsWithGrantedTags(FrozenTags);
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GAS][Frozen] After=%d"),
		ASC->HasMatchingGameplayTag(
			DRGameplayTags::State_Frozen));

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false);
}
