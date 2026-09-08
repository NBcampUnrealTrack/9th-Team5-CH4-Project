#include "DRGA_BarrierSkill.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/Barrier/DRBarrierGenerator.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"

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
	FTransform SpawnTransform;
	if (!IsValid(Character) || !BarrierGeneratorClass
		|| !ResolveBarrierSpawnTransform(Character, SpawnTransform)
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
		const float MaxHealthMultiplierBonus = IsValid(PerkComponent)
			? PerkComponent->GetSkillEffectValue(
				SkillId,
				EDRSkillEffectTrigger::OnSkillCommitted,
				DRGameplayTags::Data_Perk_Barrier_MaxHealthMultiplier)
			: 0.f;
		TArray<FGameplayEffectSpecHandle> AreaEffectSpecs;
		if (IsValid(PerkComponent))
		{
			PerkComponent->BuildEquippedSkillEffectSpecs(
				ActorInfo->AbilitySystemComponent.Get(),
				SkillId,
				EDRSkillEffectTrigger::OnSkillCommitted,
				AreaEffectSpecs);
		}

		UWorld* World = Character->GetWorld();
		ADRBarrierGenerator* BarrierGenerator = IsValid(World)
			? World->SpawnActorDeferred<ADRBarrierGenerator>(
			BarrierGeneratorClass,
			SpawnTransform,
			Character,
			Character,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;
		if (!IsValid(BarrierGenerator))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		BarrierGenerator->Initialize(
			Character,
			BarrierRadius,
			BarrierDuration,
			BarrierMaxHealth * FMath::Max(0.f, 1.f + MaxHealthMultiplierBonus),
			AreaEffectSpecs);
		BarrierGenerator->FinishSpawning(SpawnTransform);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UDRGA_BarrierSkill::ResolveBarrierSpawnTransform(
	ADRPlayerCharacter* Character,
	FTransform& OutSpawnTransform) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	const float CapsuleHalfHeight =
		Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector SpawnLocation =
		Character->GetActorLocation() - FVector::UpVector * CapsuleHalfHeight;
	OutSpawnTransform = FTransform(Character->GetActorRotation(), SpawnLocation);
	return true;
}
