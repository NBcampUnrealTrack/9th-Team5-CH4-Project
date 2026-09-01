#include "DRGA_HotPackSkill.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Skill/DRHotPackArea.h"

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
			HotPackArea->Initialize(Character);
			HotPackArea->FinishSpawning(SpawnTransform);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
