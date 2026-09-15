#include "DRGA_HotPackSkill.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/DRHotPackArea.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"

void UDRGA_HotPackSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	if (!IsValid(Character)
		|| !HotPackAreaClass
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		const ADRPlayerState* PlayerState = Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get());
		const UDRPerkComponent* PerkComponent = IsValid(PlayerState)
			? PlayerState->GetPerkComponent()
			: nullptr;
		const UDRSkillDefinition* SkillDefinition = GetCurrentSkillDefinition();
		const FGameplayTag SkillId = IsValid(SkillDefinition)
			? SkillDefinition->SkillId
			: FGameplayTag();
		const float AreaRadiusBonus = IsValid(PerkComponent)
			? PerkComponent->GetSkillEffectValue(
				SkillId,
				EDRSkillEffectTrigger::OnSkillCommitted,
				DRGameplayTags::Data_Perk_HotPack_AreaRadius)
			: 0.0f;
		const float AreaDurationBonus = IsValid(PerkComponent)
			? PerkComponent->GetSkillEffectValue(
				SkillId,
				EDRSkillEffectTrigger::OnSkillCommitted,
				DRGameplayTags::Data_Perk_HotPack_AreaDuration)
			: 0.0f;
		const float ModifiedAreaRadius = AreaRadius + AreaRadiusBonus;
		const float ModifiedAreaDuration = AreaDuration + AreaDurationBonus;
		const float HealthRecoveryBonus = IsValid(PerkComponent)
			? PerkComponent->GetSkillEffectValue(
				SkillId,
				EDRSkillEffectTrigger::OnSkillCommitted,
				DRGameplayTags::Data_Perk_HotPack_HealthRecovery)
			: 0.0f;
		const float FreezeGaugeRecoveryBonus = IsValid(PerkComponent)
			? PerkComponent->GetSkillEffectValue(
				SkillId,
				EDRSkillEffectTrigger::OnSkillCommitted,
				DRGameplayTags::Data_Perk_HotPack_FreezeGaugeRecovery)
			: 0.0f;
		const float ModifiedHealthRecoveryAmount =
			HealthRecoveryAmount + HealthRecoveryBonus;
		const float ModifiedFreezeGaugeRecoveryAmount =
			FreezeGaugeRecoveryAmount + FreezeGaugeRecoveryBonus;
		const float CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector SpawnLocation = Character->GetActorLocation() - FVector::UpVector * CapsuleHalfHeight;
		const FTransform SpawnTransform(Character->GetActorRotation(), SpawnLocation);
		ADRHotPackArea* HotPackArea = Character->GetWorld()->SpawnActorDeferred<ADRHotPackArea>(
			HotPackAreaClass,
			SpawnTransform,
			Character,
			Character,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (IsValid(HotPackArea))
		{
			HotPackArea->Initialize(
				Character,
				ModifiedAreaRadius,
				ModifiedAreaDuration,
				RecoveryEffectClass,
				ModifiedHealthRecoveryAmount,
				ModifiedFreezeGaugeRecoveryAmount);
			HotPackArea->FinishSpawning(SpawnTransform);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
