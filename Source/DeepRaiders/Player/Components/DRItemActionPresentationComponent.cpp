#include "DRItemActionPresentationComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"

#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FName FirstPersonSwingTrackName(
		TEXT("FirstPersonSwing"));
}

UDRItemActionPresentationComponent::UDRItemActionPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
}

void UDRItemActionPresentationComponent::BeginPlay()
{
	Super::BeginPlay();

	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	USceneComponent* EquipmentRoot =
		Character->GetFirstPersonEquipmentRoot();

	if (!IsValid(EquipmentRoot))
	{
		return;
	}

	EquipmentRootBaseTransform =
		EquipmentRoot->GetRelativeTransform();

	/*
	 * Timeline Track을 최초 1회 생성할 Curve.
	 * 실제 Action 실행 시 SetFloatCurve로
	 * Dig / Melee Curve를 교체한다.
	 */
	UCurveFloat* InitialCurve = nullptr;

	if (IsValid(
			FirstPersonDigPresentation.Curve))
	{
		InitialCurve =
			FirstPersonDigPresentation.Curve;
	}
	else if (IsValid(
				 FirstPersonMeleePresentation.Curve))
	{
		InitialCurve =
			FirstPersonMeleePresentation.Curve;
	}

	if (IsValid(InitialCurve))
	{
		InitializeSwingTimeline(
			InitialCurve);
	}
}

void UDRItemActionPresentationComponent::InitializeSwingTimeline(
	UCurveFloat* InitialCurve)
{
	if (bSwingTimelineInitialized ||
		!IsValid(InitialCurve))
	{
		return;
	}

	FOnTimelineFloat UpdateDelegate;

	UpdateDelegate.BindUFunction(
		this,
		FName("UpdateFirstPersonItemSwing"));

	FirstPersonItemSwingTimeline.AddInterpFloat(
		InitialCurve,
		UpdateDelegate,
		NAME_None,
		FirstPersonSwingTrackName);

	FOnTimelineEvent FinishedDelegate;

	FinishedDelegate.BindUFunction(
		this,
		FName("FinishFirstPersonItemSwing"));

	FirstPersonItemSwingTimeline
		.SetTimelineFinishedFunc(
			FinishedDelegate);

	FirstPersonItemSwingTimeline.SetLooping(false);

	FirstPersonItemSwingTimeline
		.SetTimelineLengthMode(
			TL_LastKeyFrame);

	bSwingTimelineInitialized = true;
}

void UDRItemActionPresentationComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(
		DeltaTime,
		TickType,
		ThisTickFunction);

	if (!FirstPersonItemSwingTimeline.IsPlaying())
	{
		SetComponentTickEnabled(false);
		return;
	}

	FirstPersonItemSwingTimeline.TickTimeline(
		DeltaTime);
}

void UDRItemActionPresentationComponent::PlayFirstPersonAction(
	EDRItemActionType ActionType)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled())
	{
		return;
	}

	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		PlayFirstPersonSwing(
			FirstPersonDigPresentation);
		break;

	case EDRItemActionType::MeleeAttack:
		PlayFirstPersonSwing(
			FirstPersonMeleePresentation);
		break;

	case EDRItemActionType::Throw:
	case EDRItemActionType::None:
	default:
		break;
	}

	/*
	 * 로컬 1인칭 Action Sound.
	 */
	USoundBase* ActionSound =
		ResolveActionSound(ActionType);

	if (IsValid(ActionSound))
	{
		UGameplayStatics::PlaySound2D(
			Character,
			ActionSound);
	}
}

void UDRItemActionPresentationComponent::PlayWorldActionFromServer(
	EDRItemActionType ActionType)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority())
	{
		return;
	}

	MulticastPlayWorldAction(
		ActionType);
}

void UDRItemActionPresentationComponent::PlayMeleeHitFeedbackFromServer(
	ADRPlayerCharacter* HitPlayer,
	bool bKilled,
	const FVector& ImpactLocation)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority() ||
		!IsValid(HitPlayer))
	{
		return;
	}

	/*
	 * 공격한 플레이어 본인 피드백.
	 */
	ClientPlayMeleeHitFeedback(
		bKilled);

	/*
	 * 맞은 플레이어의 PresentationComponent에서
	 * 해당 소유 클라이언트로 RPC.
	 */
	UDRItemActionPresentationComponent*
		HitPresentationComponent =
			HitPlayer->FindComponentByClass<
				UDRItemActionPresentationComponent>();

	if (IsValid(HitPresentationComponent))
	{
		HitPresentationComponent->
			ClientPlayMeleeDamagedFeedback(
				bKilled);
	}

	/*
	 * 모든 인스턴스에 Impact Sound 전파.
	 */
	MulticastPlayMeleeImpactSound(
		bKilled,
		ImpactLocation);
}

void UDRItemActionPresentationComponent::PlayDamagedFeedbackLocal()
{
	PlayLocalCameraShake(
		MeleeDamagedCameraShakeClass,
		1.f);
}

void UDRItemActionPresentationComponent::PlayFirstPersonSwing(
	const FDRFirstPersonSwingPresentation&
		Presentation)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled() ||
		!IsValid(Presentation.Curve))
	{
		return;
	}

	USceneComponent* EquipmentRoot =
		Character->GetFirstPersonEquipmentRoot();

	if (!IsValid(EquipmentRoot))
	{
		return;
	}

	/*
	 * BeginPlay 시 Curve가 없었더라도
	 * 실행 시점에 설정되어 있으면 초기화 가능.
	 */
	if (!bSwingTimelineInitialized)
	{
		InitializeSwingTimeline(
			Presentation.Curve);
	}

	if (!bSwingTimelineInitialized)
	{
		return;
	}

	FirstPersonItemSwingTimeline.Stop();

	EquipmentRoot->SetRelativeTransform(
		EquipmentRootBaseTransform);

	ActiveSwingRotation =
		Presentation.RotationOffset;

	ActiveSwingLocation =
		Presentation.LocationOffset;

	FirstPersonItemSwingTimeline.SetFloatCurve(
		Presentation.Curve,
		FirstPersonSwingTrackName);

	SetComponentTickEnabled(true);

	FirstPersonItemSwingTimeline.PlayFromStart();
}

void UDRItemActionPresentationComponent::UpdateFirstPersonItemSwing(
	float CurveValue)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled())
	{
		return;
	}

	USceneComponent* EquipmentRoot =
		Character->GetFirstPersonEquipmentRoot();

	if (!IsValid(EquipmentRoot))
	{
		return;
	}

	const FVector BaseLocation =
		EquipmentRootBaseTransform.GetLocation();

	const FRotator BaseRotation =
		EquipmentRootBaseTransform.Rotator();

	const FVector NewLocation =
		BaseLocation +
		ActiveSwingLocation * CurveValue;

	const FRotator RotationOffset =
		ActiveSwingRotation * CurveValue;

	const FRotator NewRotation =
		BaseRotation + RotationOffset;

	EquipmentRoot->
		SetRelativeLocationAndRotation(
			NewLocation,
			NewRotation);
}

void UDRItemActionPresentationComponent::FinishFirstPersonItemSwing()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		SetComponentTickEnabled(false);
		return;
	}

	USceneComponent* EquipmentRoot =
		Character->GetFirstPersonEquipmentRoot();

	if (IsValid(EquipmentRoot))
	{
		EquipmentRoot->SetRelativeTransform(
			EquipmentRootBaseTransform);
	}

	SetComponentTickEnabled(false);
}

void UDRItemActionPresentationComponent::PlayWorldAction(
	EDRItemActionType ActionType)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	UAnimMontage* Montage =
		ResolveWorldActionMontage(
			ActionType);

	if (!IsValid(Montage))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[ItemAction] "
				"World montage is invalid. "
				"Character=%s Action=%s"),
			*GetNameSafe(Character),
			*UEnum::GetValueAsString(
				ActionType));

		return;
	}

	Character->PlayAnimMontage(
		Montage);
}

UAnimMontage* UDRItemActionPresentationComponent::ResolveWorldActionMontage(
	EDRItemActionType ActionType) const
{
	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		return WorldDigMontage;

	case EDRItemActionType::MeleeAttack:
		return WorldMeleeAttackMontage;

	case EDRItemActionType::Throw:
	case EDRItemActionType::None:
	default:
		return nullptr;
	}
}

USoundBase* UDRItemActionPresentationComponent::ResolveActionSound(
	EDRItemActionType ActionType) const
{
	switch (ActionType)
	{
	case EDRItemActionType::Dig:
		return DigSound;

	case EDRItemActionType::MeleeAttack:
		return MeleeAirSound;

	case EDRItemActionType::Throw:
	case EDRItemActionType::None:
	default:
		return nullptr;
	}
}

void UDRItemActionPresentationComponent::PlayLocalCameraShake(
	TSubclassOf<UCameraShakeBase> ShakeClass,
	float Scale)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled() ||
		!ShakeClass)
	{
		return;
	}

	APlayerController* PlayerController =
		Cast<APlayerController>(
			Character->GetController());

	if (!IsValid(PlayerController) ||
		!IsValid(
			PlayerController->PlayerCameraManager))
	{
		return;
	}

	PlayerController->
		PlayerCameraManager->
		StartCameraShake(
			ShakeClass,
			Scale,
			ECameraShakePlaySpace::CameraLocal,
			FRotator::ZeroRotator);
}

void UDRItemActionPresentationComponent::MulticastPlayMeleeImpactSound_Implementation(
	bool bKilled,
	FVector_NetQuantize ImpactLocation)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	USoundBase* SoundToPlay =
		bKilled
			? MeleeKillSound
			: MeleeHitSound;

	if (!IsValid(SoundToPlay))
	{
		return;
	}

	/*
	 * 공격한 본인은 1인칭 피드백이므로
	 * 2D Sound.
	 */
	if (Character->IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(
			Character,
			SoundToPlay);

		return;
	}

	/*
	 * 나머지 플레이어는
	 * 실제 Impact 위치에서 3D Sound.
	 */
	UGameplayStatics::PlaySoundAtLocation(
		Character,
		SoundToPlay,
		ImpactLocation);
}

void UDRItemActionPresentationComponent::ClientPlayMeleeDamagedFeedback_Implementation(
	bool bKilled)
{
	PlayLocalCameraShake(
		MeleeDamagedCameraShakeClass,
		bKilled ? 1.2f : 1.f);
}

void UDRItemActionPresentationComponent::ClientPlayMeleeHitFeedback_Implementation(
	bool bKilled)
{
	PlayLocalCameraShake(
		MeleeHitConfirmCameraShakeClass,
		bKilled ? 1.3f : 1.f);
}

void UDRItemActionPresentationComponent::MulticastPlayWorldAction_Implementation(
	EDRItemActionType ActionType)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	/*
	 * 서버는 WeaponSweep Socket 판정을 위해
	 * Listen Host의 로컬 캐릭터라도
	 * World Montage를 재생해야 한다.
	 *
	 * Remote Client도 다른 플레이어의
	 * World Montage를 재생한다.
	 */
	if (Character->HasAuthority() ||
		!Character->IsLocallyControlled())
	{
		PlayWorldAction(
			ActionType);
	}

	/*
	 * 소유 로컬 플레이어는 이미
	 * 1P Action Sound를 재생했다.
	 *
	 * World Sound까지 재생하면
	 * 자기 액션 사운드가 두 번 들린다.
	 */
	if (Character->IsLocallyControlled())
	{
		return;
	}

	USoundBase* ActionSound =
		ResolveActionSound(
			ActionType);

	if (!IsValid(ActionSound))
	{
		return;
	}

	/*
	 * 다른 플레이어는
	 * 실제 캐릭터 위치에서 3D Sound.
	 */
	UGameplayStatics::PlaySoundAtLocation(
		Character,
		ActionSound,
		Character->GetActorLocation());
}

ADRPlayerCharacter* UDRItemActionPresentationComponent::GetOwnerCharacter() const
{
	return Cast<ADRPlayerCharacter>(
		GetOwner());
}

