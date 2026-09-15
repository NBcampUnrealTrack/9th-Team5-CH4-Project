#include "DRAN_MeleeSweepSample.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRMeleeCombatComponent.h"

UDRAN_MeleeSweepSample::UDRAN_MeleeSweepSample()
{
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

	HandleSweepSample(
		MeshComp,
		Animation,
		const_cast<FAnimNotifyEvent*>(
			EventReference.GetNotify()));
}

void UDRAN_MeleeSweepSample::BranchingPointNotify(
	FBranchingPointNotifyPayload& BranchingPointPayload)
{
	/*
	 * bIsNativeBranchingPoint == true 이므로
	 * Montage에서는 이 경로를 명시적으로 처리한다.
	 *
	 * 여기서는 Super 호출하지 않는다.
	 * Super가 일반 Notify 경로를 다시 호출할 가능성을
	 * 없애 중복 Sample을 방지한다.
	 */
	HandleSweepSample(
		BranchingPointPayload.SkelMeshComponent,
		BranchingPointPayload.SequenceAsset,
		BranchingPointPayload.NotifyEvent);
}

void UDRAN_MeleeSweepSample::HandleSweepSample(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	FAnimNotifyEvent* NotifyEvent)
{
	if (!IsValid(MeshComp) ||
		!IsValid(Animation))
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

	if (!Character->HasAuthority())
	{
		return;
	}

	if (NotifyEvent == nullptr)
	{
		return;
	}

	UDRMeleeCombatComponent* Melee =
		Character->GetMeleeCombatComponent();

	if (!IsValid(Melee))
	{
		return;
	}

	const float SampleTime =
		NotifyEvent->GetTriggerTime();

	Melee->SampleWeaponSweep(
		Animation,
		SampleTime);
}