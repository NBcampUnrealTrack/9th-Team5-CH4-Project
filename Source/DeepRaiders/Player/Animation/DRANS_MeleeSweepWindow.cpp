#include "DRANS_MeleeSweepWindow.h"

#include "Components/SkeletalMeshComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

void UDRANS_MeleeSweepWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(
		MeshComp,
		Animation,
		TotalDuration,
		EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(
			MeshComp->GetOwner());

	if (!IsValid(Character))
	{
		return;
	}

	Character->StartMeleeWeaponSweep();
}

void UDRANS_MeleeSweepWindow::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(
		MeshComp,
		Animation,
		FrameDeltaTime,
		EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(
			MeshComp->GetOwner());

	if (!IsValid(Character))
	{
		return;
	}

	Character->UpdateMeleeWeaponSweep();
}

void UDRANS_MeleeSweepWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(
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

	if (!IsValid(Character))
	{
		return;
	}

	Character->StopMeleeWeaponSweep();
}