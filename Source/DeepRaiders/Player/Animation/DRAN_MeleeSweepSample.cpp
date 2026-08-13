#include "DRAN_MeleeSweepSample.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRMeleeCombatComponent.h"

UDRAN_MeleeSweepSample::
UDRAN_MeleeSweepSample()
{
	/*
	 * Montage에서 사용될 경우
	 * 항상 Branching Point로 처리.
	 */
	bIsNativeBranchingPoint = true;
}

void UDRAN_MeleeSweepSample::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(
		MeshComp,
		Animation,
		EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(
			MeshComp->GetOwner());

	if (!IsValid(Character) ||
		!Character->HasAuthority())
	{
		return;
	}

	UDRMeleeCombatComponent* Melee =
		Character->GetMeleeCombatComponent();

	if (IsValid(Melee))
	{
		Melee->SampleWeaponSweep();
	}
}