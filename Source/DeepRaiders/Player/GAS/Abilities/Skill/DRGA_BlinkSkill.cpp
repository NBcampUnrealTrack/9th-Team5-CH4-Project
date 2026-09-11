#include "DRGA_BlinkSkill.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Controller.h"

#include "DeepRaiders/GAS/Cues/DRGameplayCuePresentationLibrary.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/Skill/Effects/DRGE_BlinkRecovery.h"

void UDRGA_BlinkSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ADRPlayerCharacter* Character = GetPlayerCharacter(ActorInfo);
	if (!IsValid(Character))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector BlinkDirection = Character->GetSkillMovementDirection();
	if (BlinkDirection.IsNearlyZero())
	{
		const AController* Controller = Character->GetController();
		if (Controller != nullptr)
		{
			BlinkDirection = Controller->GetControlRotation().Vector().GetSafeNormal2D();
		}
	}

	if (BlinkDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector BlinkLocation = Character->GetActorLocation();
	const FVector Destination = BlinkLocation + BlinkDirection * BlinkDistance;
	const FRotator CharacterRotation = Character->GetActorRotation();
	FHitResult HitResult;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Sweep the character's collision capsule so a blocking wall clamps the blink
	// to the first impact instead of allowing teleport destination adjustment past it.
	Character->SetActorLocation(Destination, true, &HitResult, ETeleportType::TeleportPhysics);
	Character->SetActorRotation(CharacterRotation, ETeleportType::TeleportPhysics);
	PlayBlinkVFX(Character, BlinkLocation, BlinkDirection);

	if (RecoveryDuration > 0.0f)
	{
		FGameplayEffectSpecHandle RecoverySpec = MakeOutgoingGameplayEffectSpec(
			Handle,
			ActorInfo,
			ActivationInfo,
			UDRGE_BlinkRecovery::StaticClass(),
			GetAbilityLevel(Handle, ActorInfo));

		if (RecoverySpec.IsValid())
		{
			RecoverySpec.Data->SetSetByCallerMagnitude(
				DRGameplayTags::Data_BlinkRecovery_Duration,
				RecoveryDuration);
			ApplyGameplayEffectSpecToOwner(
				Handle,
				ActorInfo,
				ActivationInfo,
				RecoverySpec);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UDRGA_BlinkSkill::PlayBlinkVFX(
	ADRPlayerCharacter* Character,
	const FVector& BlinkLocation,
	const FVector& BlinkDirection) const
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(Character)
		|| !IsValid(AbilitySystem))
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = BlinkLocation;
	Parameters.Normal = BlinkDirection.GetSafeNormal2D();
	Parameters.Instigator = Character;
	Parameters.EffectCauser = Character;
	Parameters.SourceObject = GetCurrentSkillDefinition();

	AbilitySystem->ExecuteGameplayCue(
		DRGameplayTags::GameplayCue_VFX_Skill_Blink,
		Parameters);
	
	AbilitySystem->ExecuteGameplayCue(
		DRGameplayTags::GameplayCue_VFX_Skill_Blink_End,
		Parameters);
	
	UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(
		Character,
		DRGameplayTags::GameplayCue_Sound_Skill_Blink,
		Parameters);
}
