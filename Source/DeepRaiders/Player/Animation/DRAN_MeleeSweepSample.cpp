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
	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[V4][Notify] Normal Notify ENTER "
			"Mesh=%s Animation=%s"),
		*GetNameSafe(MeshComp),
		*GetNameSafe(Animation));

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

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[V4][Notify] BRANCHING POINT ENTER "
			"Mesh=%s Animation=%s "
			"NotifyEvent=%s"),
		*GetNameSafe(
			BranchingPointPayload.SkelMeshComponent),
		*GetNameSafe(
			BranchingPointPayload.SequenceAsset),
		BranchingPointPayload.NotifyEvent
			? TEXT("VALID")
			: TEXT("NULL"));

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
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[V4][Notify] Invalid Mesh/Animation"));

		return;
	}

	ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(
			MeshComp->GetOwner());

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[V4][Notify] Owner=%s "
			"Authority=%d "
			"Local=%d "
			"NetMode=%d"),
		*GetNameSafe(Character),
		IsValid(Character)
			? Character->HasAuthority()
			: false,
		IsValid(Character)
			? Character->IsLocallyControlled()
			: false,
		MeshComp->GetWorld()
			? static_cast<int32>(
				MeshComp->GetWorld()->GetNetMode())
			: -1);

	if (!IsValid(Character))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[V4][Notify] Owner is not "
				"ADRPlayerCharacter"));

		return;
	}

	if (!Character->HasAuthority())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[V4][Notify] Skip CLIENT instance"));

		return;
	}

	if (NotifyEvent == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[V4][Notify] NotifyEvent NULL"));

		return;
	}

	UDRMeleeCombatComponent* Melee =
		Character->GetMeleeCombatComponent();

	if (!IsValid(Melee))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[V4][Notify] MeleeComponent INVALID"));

		return;
	}

	const float SampleTime =
		NotifyEvent->GetTriggerTime();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[V4][Notify] CALL SAMPLE "
			"Time=%.4f Animation=%s"),
		SampleTime,
		*GetNameSafe(Animation));

	Melee->SampleWeaponSweep(
		Animation,
		SampleTime);
}