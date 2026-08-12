#include "DRANS_MeleeSweepWindow.h"

#include "Components/SkeletalMeshComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRMeleeCombatComponent.h"

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

	if (ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(
			MeshComp->GetOwner()))
	{
		if (UDRMeleeCombatComponent* Combat =
				Character->GetMeleeCombatComponent())
		{
			Combat->StartSweepWindow();
		}
	}
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

	if (ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(
			MeshComp->GetOwner()))
	{
		if (UDRMeleeCombatComponent* Combat =
				Character->GetMeleeCombatComponent())
		{
			Combat->UpdateSweepWindow();
		}
	}
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

	if (ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(
			MeshComp->GetOwner()))
	{
		if (UDRMeleeCombatComponent* Combat =
				Character->GetMeleeCombatComponent())
		{
			Combat->EndSweepWindow();
		}
	}
}