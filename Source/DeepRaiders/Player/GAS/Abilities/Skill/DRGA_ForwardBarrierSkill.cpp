#include "DRGA_ForwardBarrierSkill.h"

#include "Components/CapsuleComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

bool UDRGA_ForwardBarrierSkill::ResolveBarrierSpawnTransform(
	ADRPlayerCharacter* Character,
	FTransform& OutSpawnTransform) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	const FVector ForwardDirection = Character->GetActorForwardVector();
	const float CapsuleHalfHeight =
		Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector SpawnLocation = Character->GetActorLocation()
		+ ForwardDirection * ForwardDistance
		- FVector::UpVector * CapsuleHalfHeight;
	OutSpawnTransform = FTransform(ForwardDirection.Rotation(), SpawnLocation);
	return true;
}
