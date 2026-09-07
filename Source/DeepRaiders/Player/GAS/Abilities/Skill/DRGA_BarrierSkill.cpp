#include "DRGA_BarrierSkill.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Skill/Barrier/DRBarrierGenerator.h"

UDRGA_BarrierSkill::UDRGA_BarrierSkill()
{
	FGameplayTagContainer BarrierAbilityTags;
	BarrierAbilityTags.AddTag(DRGameplayTags::Ability_Skill);
	BarrierAbilityTags.AddTag(DRGameplayTags::Ability_Skill_Barrier);
	SetAssetTags(BarrierAbilityTags);

	BarrierGeneratorClass = ADRBarrierGenerator::StaticClass();
}

void UDRGA_BarrierSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	if (!IsValid(Character) || !BarrierGeneratorClass
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		const float CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector SpawnLocation = Character->GetActorLocation() - FVector::UpVector * CapsuleHalfHeight;
		const FTransform SpawnTransform(Character->GetActorRotation(), SpawnLocation);
		ADRBarrierGenerator* Generator = Character->GetWorld()->SpawnActorDeferred<ADRBarrierGenerator>(
			BarrierGeneratorClass, SpawnTransform, Character, Character,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!IsValid(Generator))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		Generator->Initialize(Character, BarrierRadius, BarrierDuration, BarrierMaxHealth);
		Generator->FinishSpawning(SpawnTransform);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
