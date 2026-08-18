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

void UDRItemActionPresentationComponent::PlayWeaponFireLocal(UAnimMontage* FireMontage)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->IsLocallyControlled() || !IsValid(FireMontage))
	{
		return;
	}

	Character->PlayAnimMontage(FireMontage);
}

void UDRItemActionPresentationComponent::PlayWeaponFireFromServer(UAnimMontage* FireMontage)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority() || !IsValid(FireMontage))
	{
		return;
	}

	MulticastPlayWeaponFire(FireMontage);
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

void UDRItemActionPresentationComponent::MulticastPlayWeaponFire_Implementation(UAnimMontage* FireMontage)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !IsValid(FireMontage))
	{
		return;
	}

	/*
	 * Remote Owner는 LocalPredicted GA에서
	 * 이미 즉시 재생했으므로 중복 재생하지 않는다.
	 *
	 * Listen Host는 서버 인스턴스에서 재생한다.
	 */
	if (Character->IsLocallyControlled() && !Character->HasAuthority())
	{
		return;
	}

	if (Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	Character->PlayAnimMontage(FireMontage);
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

	PlayWorldAction(ActionType);

	USoundBase* ActionSound =
		ResolveActionSound(ActionType);

	if (!IsValid(ActionSound))
	{
		return;
	}

	if (Character->IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(
			Character,
			ActionSound);

		return;
	}

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

