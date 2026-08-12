#include "DRJetpackComponent.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"

#include "Components/AudioComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UDRJetpackComponent::UDRJetpackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
}

void UDRJetpackComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(
		OutLifetimeProps);

	DOREPLIFETIME(
		UDRJetpackComponent,
		bIsJetpackActive);
}

void UDRJetpackComponent::HandleLanded()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	if (Character->IsLocallyControlled())
	{
		StopLocalPrediction();
	}

	if (Character->HasAuthority())
	{
		StopFromServer();
	}
}

void UDRJetpackComponent::HandlePlayerStateReady()
{
	RefreshVisual();
	InitializeLocalFuelPrediction();
}

void UDRJetpackComponent::RefreshVisual()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	const ADRPlayerState* DRPlayerState =
		Character->GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		Character->ClearBackEquipmentVisual();
		return;
	}

	if (DRPlayerState->HasJetpack())
	{
		Character->ApplyBackEquipmentVisual(
			JetpackMesh,
			JetpackRelativeTransform);
	}
	else
	{
		Character->ClearBackEquipmentVisual();
	}
}

float UDRJetpackComponent::GetDisplayedFuelRatio() const
{
	const ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return 0.f;
	}

	const ADRPlayerState* DRPlayerState =
		Character->GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return 0.f;
	}

	const float MaxFuel =
		DRPlayerState->GetMaxJetpackFuel();

	if (MaxFuel <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	/*
	 * 서버 / Listen Host는 권위값.
	 * 소유 게스트만 예측 표시값.
	 */
	if (Character->HasAuthority() ||
		!Character->IsLocallyControlled() ||
		!bLocalFuelPredictionInitialized)
	{
		return DRPlayerState->
			GetJetpackFuelRatio();
	}

	return FMath::Clamp(
		LocalPredictedFuel / MaxFuel,
		0.f,
		1.f);
}

void UDRJetpackComponent::ReconcileFuelFromServer(
	float ServerFuel)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled() ||
		Character->HasAuthority())
	{
		return;
	}

	const UDRCharacterMovementComponent* Movement =
		GetDRMovementComponent();

	const bool bLocallyUsingJetpack =
		IsValid(Movement) &&
		Movement->WantsJetpack();

	/*
	 * 사용 중에는 지연되어 도착한 snapshot을
	 * 덮어쓰지 않는다.
	 */
	if (bLocallyUsingJetpack &&
		ServerFuel > KINDA_SMALL_NUMBER)
	{
		return;
	}

	LocalPredictedFuel =
		FMath::Max(
			0.f,
			ServerFuel);

	bLocalFuelPredictionInitialized =
		true;
}

ADRPlayerCharacter* UDRJetpackComponent::GetOwnerCharacter() const
{
	return Cast<ADRPlayerCharacter>(
		GetOwner());
}

UDRCharacterMovementComponent* UDRJetpackComponent::GetDRMovementComponent() const
{
	const ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return nullptr;
	}

	return Cast<UDRCharacterMovementComponent>(
		Character->GetCharacterMovement());
}

void UDRJetpackComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(
		DeltaTime,
		TickType,
		ThisTickFunction);

	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	/*
	 * 실제 Fuel은 서버만 소비.
	 */
	if (Character->HasAuthority())
	{
		if (bIsJetpackActive)
		{
			UpdateFuel(DeltaTime);
		}

		return;
	}

	/*
	 * 소유 게스트는 HUD 표시값만 로컬 예측한다.
	 */
	if (!Character->IsLocallyControlled() ||
		!bLocalFuelPredictionInitialized)
	{
		return;
	}

	const UDRCharacterMovementComponent* Movement =
		GetDRMovementComponent();

	if (!IsValid(Movement) ||
		!Movement->WantsJetpack())
	{
		return;
	}

	LocalPredictedFuel =
		FMath::Max(
			0.f,
			LocalPredictedFuel -
			FuelConsumptionPerSecond *
			DeltaTime);
}

void UDRJetpackComponent::RequestStart()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled() ||
		Character->IsDead())
	{
		return;
	}

	UDRCharacterMovementComponent* Movement =
		GetDRMovementComponent();

	ADRPlayerState* DRPlayerState =
		Character->GetPlayerState<ADRPlayerState>();

	if (!IsValid(Movement) ||
		!Movement->IsFalling() ||
		!IsValid(DRPlayerState) ||
		!DRPlayerState->HasJetpack() ||
		DRPlayerState->GetJetpackFuel() <=
			KINDA_SMALL_NUMBER)
	{
		return;
	}

	/*
	 * 서버 응답을 기다리지 않고
	 * CharacterMovement Prediction 즉시 시작.
	 */
	Movement->SetWantsJetpack(true);

	/*
	 * Listen Host는 실제 서버값을 사용하므로
	 * 로컬 표시 Prediction은 게스트만 필요.
	 */
	if (!Character->HasAuthority())
	{
		if (!bLocalFuelPredictionInitialized)
		{
			InitializeLocalFuelPrediction();
		}

		SetComponentTickEnabled(true);
	}

	RefreshActivePresentation();

	ServerStartJetpack();
}

void UDRJetpackComponent::RequestStop()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled())
	{
		return;
	}

	UDRCharacterMovementComponent* Movement =
		GetDRMovementComponent();

	if (IsValid(Movement))
	{
		Movement->SetWantsJetpack(false);
	}

	/*
	 * Guest의 Component Tick은 UI Prediction용이므로
	 * 즉시 중단.
	 *
	 * Host는 ServerStop에서 서버 Tick이 꺼진다.
	 */
	if (!Character->HasAuthority())
	{
		SetComponentTickEnabled(false);
	}

	RefreshActivePresentation();

	ServerStopJetpack();
}

bool UDRJetpackComponent::CanStartJetpack() const
{
	const ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority() ||
		Character->IsDead())
	{
		return false;
	}

	const UCharacterMovementComponent* Movement =
		Character->GetCharacterMovement();

	if (!IsValid(Movement) ||
		!Movement->IsFalling())
	{
		return false;
	}

	const ADRPlayerState* DRPlayerState =
		Character->GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return false;
	}

	return
		DRPlayerState->HasJetpack() &&
		DRPlayerState->GetJetpackFuel() > 0.f;
}

void UDRJetpackComponent::ServerStartJetpack_Implementation()
{
	if (!CanStartJetpack())
	{
		ClientRejectJetpack();
		return;
	}

	StartFromServer();
}

void UDRJetpackComponent::ServerStopJetpack_Implementation()
{
	StopFromServer();
}

void UDRJetpackComponent::StartFromServer()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority() ||
		bIsJetpackActive)
	{
		return;
	}

	UDRCharacterMovementComponent* Movement =
		GetDRMovementComponent();

	if (IsValid(Movement))
	{
		Movement->SetWantsJetpack(true);
	}

	bIsJetpackActive = true;

	/*
	 * 서버 연료 소비용 Component Tick.
	 * Character Tick을 더 이상 서버 연료 소비에 쓰지 않는다.
	 */
	SetComponentTickEnabled(true);

	RefreshActivePresentation();

	Character->ForceNetUpdate();
}

void UDRJetpackComponent::StopFromServer()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority())
	{
		return;
	}

	UDRCharacterMovementComponent* Movement =
		GetDRMovementComponent();

	if (IsValid(Movement))
	{
		Movement->SetWantsJetpack(false);
	}

	SetComponentTickEnabled(false);

	if (!bIsJetpackActive)
	{
		return;
	}

	bIsJetpackActive = false;

	RefreshActivePresentation();

	Character->ForceNetUpdate();
}

void UDRJetpackComponent::UpdateFuel(
	float DeltaSeconds)
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority())
	{
		return;
	}

	UDRCharacterMovementComponent* Movement =
		GetDRMovementComponent();

	ADRPlayerState* DRPlayerState =
		Character->GetPlayerState<ADRPlayerState>();

	if (!IsValid(Movement) ||
		!IsValid(DRPlayerState) ||
		!Movement->IsFalling() ||
		!Movement->WantsJetpack() ||
		!DRPlayerState->HasJetpack())
	{
		StopFromServer();
		return;
	}

	const float FuelCost =
		FuelConsumptionPerSecond *
		DeltaSeconds;

	if (!DRPlayerState->ConsumeJetpackFuel(
			FuelCost))
	{
		StopFromServer();
		ClientRejectJetpack();
		return;
	}

	if (DRPlayerState->GetJetpackFuel() <=
		KINDA_SMALL_NUMBER)
	{
		StopFromServer();
		ClientRejectJetpack();
	}
}

void UDRJetpackComponent::ClientRejectJetpack_Implementation()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	StopLocalPrediction();

	/*
	 * 현재 수신해둔 권위 Fuel로 다시 보정.
	 */
	const ADRPlayerState* DRPlayerState =
		Character->GetPlayerState<ADRPlayerState>();

	if (IsValid(DRPlayerState))
	{
		LocalPredictedFuel =
			DRPlayerState->GetJetpackFuel();

		bLocalFuelPredictionInitialized =
			true;
	}
}

void UDRJetpackComponent::OnRep_JetpackActive()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character))
	{
		return;
	}

	/*
	 * 서버에서 연료 소진 / 착지 / 사망 등으로
	 * Jetpack을 강제 종료한 경우
	 * 소유 클라이언트 Prediction도 정리.
	 */
	if (!bIsJetpackActive &&
		Character->IsLocallyControlled())
	{
		StopLocalPrediction();
		return;
	}

	RefreshActivePresentation();
}

void UDRJetpackComponent::InitializeLocalFuelPrediction()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled())
	{
		return;
	}

	const ADRPlayerState* DRPlayerState =
		Character->GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return;
	}

	LocalPredictedFuel =
		DRPlayerState->GetJetpackFuel();

	bLocalFuelPredictionInitialized = true;
}

void UDRJetpackComponent::RefreshActivePresentation()
{
    ADRPlayerCharacter* Character =
        GetOwnerCharacter();

    if (!IsValid(Character))
    {
        return;
    }

    const UDRCharacterMovementComponent* Movement =
        GetDRMovementComponent();

    const bool bPresentationActive =
        Character->IsLocallyControlled()
            ? IsValid(Movement) &&
                Movement->WantsJetpack()
            : bIsJetpackActive;

    // ==============================
    // Sound
    // ==============================

    if (bPresentationActive)
    {
        if (!IsValid(JetpackAudioComponent) &&
            IsValid(JetpackSound))
        {
            JetpackAudioComponent =
                UGameplayStatics::SpawnSoundAttached(
                    JetpackSound,
                    Character->GetRootComponent());
        }
    }
    else
    {
        if (IsValid(JetpackAudioComponent))
        {
            JetpackAudioComponent->Stop();
            JetpackAudioComponent = nullptr;
        }
    }

    // Camera Shake는 자기 화면만.
    if (!Character->IsLocallyControlled())
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

    if (bPresentationActive)
    {
        if (!IsValid(
                JetpackCameraShakeInstance) &&
            JetpackCameraShakeClass)
        {
            JetpackCameraShakeInstance =
                PlayerController->
                PlayerCameraManager->
                StartCameraShake(
                    JetpackCameraShakeClass,
                    1.f,
                    ECameraShakePlaySpace::
                        CameraLocal,
                    FRotator::ZeroRotator);
        }
    }
    else
    {
        if (IsValid(
                JetpackCameraShakeInstance))
        {
            PlayerController->
                PlayerCameraManager->
                StopCameraShake(
                    JetpackCameraShakeInstance,
                    false);

            JetpackCameraShakeInstance =
                nullptr;
        }
    }
}

void UDRJetpackComponent::StopLocalPrediction()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled())
	{
		return;
	}

	UDRCharacterMovementComponent* Movement =
		GetDRMovementComponent();

	if (IsValid(Movement))
	{
		Movement->SetWantsJetpack(false);
	}

	if (!Character->HasAuthority())
	{
		SetComponentTickEnabled(false);
	}

	RefreshActivePresentation();
}
