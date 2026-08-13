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
	InitializeFuelDisplayFromServer();
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
		Character->GetPlayerState<
			ADRPlayerState>();

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
	 * 서버/Listen Host는 권위값.
	 */
	if (Character->HasAuthority())
	{
		return DRPlayerState->
			GetJetpackFuelRatio();
	}

	/*
	 * 게스트는 서버 Snapshot 보간값.
	 */
	if (Character->IsLocallyControlled() &&
		bHasServerFuelSnapshot)
	{
		return FMath::Clamp(
			DisplayedFuel / MaxFuel,
			0.f,
			1.f);
	}

	return DRPlayerState->
		GetJetpackFuelRatio();
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

	ApplyServerFuelSnapshot(
		ServerFuel);
}

bool UDRJetpackComponent::RefillFuelFromServer()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->HasAuthority())
	{
		return false;
	}

	ADRPlayerState* DRPlayerState =
		Character->
			GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return false;
	}

	return DRPlayerState->
		RefillJetpackFuel();
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
	 * 서버:
	 * 실제 Fuel 소비.
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
	 * 소유 클라이언트:
	 * 서버 Snapshot 사이를 보간만 한다.
	 */
	if (Character->IsLocallyControlled())
	{
		UpdateFuelInterpolation(
			DeltaTime);
	}
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
        Character->GetPlayerState<
            ADRPlayerState>();

    if (!IsValid(Movement) ||
        !Movement->IsFalling() ||
        !IsValid(DRPlayerState) ||
        !DRPlayerState->HasJetpack())
    {
        return;
    }

    /*
     * Movement Prediction은 유지한다.
     *
     * Fuel Prediction과는 별개다.
     */
    Movement->SetWantsJetpack(true);

    RefreshActivePresentation();

    /*
     * 진짜 Fuel 검사와 사용 승인은 서버가 한다.
     */
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
		ADRPlayerCharacter* Character =
			GetOwnerCharacter();

		const ADRPlayerState* DRPlayerState =
			IsValid(Character)
			? Character->GetPlayerState<
				ADRPlayerState>()
			: nullptr;

		const float AuthoritativeFuel =
			IsValid(DRPlayerState)
			? DRPlayerState->GetJetpackFuel()
			: 0.f;

		ClientRejectJetpack(
			AuthoritativeFuel);

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
		const float AuthoritativeFuel =
			DRPlayerState->GetJetpackFuel();

		StopFromServer();

		ClientRejectJetpack(
			AuthoritativeFuel);

		return;
	}

	if (DRPlayerState->GetJetpackFuel() <=
		KINDA_SMALL_NUMBER)
	{
		const float AuthoritativeFuel =
			DRPlayerState->GetJetpackFuel();

		StopFromServer();

		ClientRejectJetpack(
			AuthoritativeFuel);
	}
}

void UDRJetpackComponent::ClientRejectJetpack_Implementation(
	float AuthoritativeFuel)
{
	/*
	 * 서버가 거절했으므로
	 * 로컬 Movement Prediction 즉시 취소.
	 */
	StopLocalPrediction();

	/*
	 * Reject RPC에 실어온 실제 서버 Fuel로
	 * HUD도 즉시 보정한다.
	 *
	 * 여기서는 Lerp하지 않는다.
	 * 사용 불가능한 상태인데 HUD에 연료가 남아 보이면
	 * 다시 혼란이 생기기 때문.
	 */
	ApplyServerFuelSnapshot(
		AuthoritativeFuel,
		true);
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

	RefreshActivePresentation();
}

void UDRJetpackComponent::InitializeFuelDisplayFromServer()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled())
	{
		return;
	}

	ADRPlayerState* DRPlayerState =
		Character->GetPlayerState<
			ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return;
	}

	const float ServerFuel =
		DRPlayerState->GetJetpackFuel();

	PreviousServerFuel =
		ServerFuel;

	CurrentServerFuel =
		ServerFuel;

	DisplayedFuel =
		ServerFuel;

	FuelLerpStartValue =
		ServerFuel;

	FuelLerpElapsed = 0.f;

	LastFuelSnapshotTime =
		GetWorld()
			? GetWorld()->GetTimeSeconds()
			: -1.f;

	bHasServerFuelSnapshot = true;
}

void UDRJetpackComponent::ApplyServerFuelSnapshot(
    float ServerFuel,
    bool bSnapImmediately)
{
    ADRPlayerCharacter* Character =
        GetOwnerCharacter();

    UWorld* World = GetWorld();

    if (!IsValid(Character) ||
        !IsValid(World))
    {
        return;
    }

    ADRPlayerState* DRPlayerState =
        Character->GetPlayerState<ADRPlayerState>();

    if (!IsValid(DRPlayerState))
    {
        return;
    }

    const float MaxFuel =
        DRPlayerState->GetMaxJetpackFuel();

    const float NewServerFuel =
        FMath::Clamp(
            ServerFuel,
            0.f,
            MaxFuel);

    const float CurrentTime =
        World->GetTimeSeconds();

    /*
     * 최초 Snapshot.
     */
    if (!bHasServerFuelSnapshot)
    {
        PreviousServerFuel =
            NewServerFuel;

        CurrentServerFuel =
            NewServerFuel;

        DisplayedFuel =
            NewServerFuel;

        FuelLerpStartValue =
            NewServerFuel;

        LastFuelSnapshotTime =
            CurrentTime;

        bHasServerFuelSnapshot =
            true;

        return;
    }

    PreviousServerFuel =
        CurrentServerFuel;

    CurrentServerFuel =
        NewServerFuel;

    /*
     * 새 Snapshot 도착 순간에 이전 Lerp가
     * 끝나지 않았을 수도 있으므로,
     *
     * PreviousServerFuel에서 강제로 시작하면
     * HUD가 순간적으로 튈 수 있다.
     *
     * 따라서 실제 화면상 현재 값에서
     * 새로운 서버 Current로 보간한다.
     */
    FuelLerpStartValue =
        DisplayedFuel;

    FuelLerpElapsed = 0.f;

    /*
     * 서버 Snapshot 도착 간격을
     * 이번 Lerp 시간으로 사용.
     */
    if (LastFuelSnapshotTime >= 0.f)
    {
        const float SnapshotInterval =
            CurrentTime -
            LastFuelSnapshotTime;

        FuelLerpDuration =
            FMath::Clamp(
                SnapshotInterval,
                0.05f,
                0.5f);
    }

    LastFuelSnapshotTime =
        CurrentTime;

    /*
     * 0이 됐는데 천천히 줄이면
     *
     * UI = 2
     * 실제 서버 = 0
     *
     * 상황이 잠깐 남게 된다.
     *
     * 연료 소진은 즉시 0으로 표시.
     */
    if (bSnapImmediately ||
        CurrentServerFuel <=
            KINDA_SMALL_NUMBER)
    {
        DisplayedFuel =
            CurrentServerFuel;

        FuelLerpElapsed =
            FuelLerpDuration;

        return;
    }

    /*
     * 착지 충전처럼 값이 증가한 경우도
     * 즉시 반영하는 편이 자연스럽다.
     */
    if (CurrentServerFuel >
        PreviousServerFuel)
    {
        DisplayedFuel =
            CurrentServerFuel;

        FuelLerpElapsed =
            FuelLerpDuration;

        return;
    }

    /*
     * 감소 Snapshot만 Lerp.
     */
    SetComponentTickEnabled(true);
}

void UDRJetpackComponent::UpdateFuelInterpolation(
	float DeltaTime)
{
	if (!bHasServerFuelSnapshot)
	{
		return;
	}

	if (FMath::IsNearlyEqual(
			DisplayedFuel,
			CurrentServerFuel,
			0.01f))
	{
		DisplayedFuel =
			CurrentServerFuel;

		SetComponentTickEnabled(false);
		return;
	}

	FuelLerpElapsed +=
		DeltaTime;

	const float Alpha =
		FuelLerpDuration >
			KINDA_SMALL_NUMBER
		? FMath::Clamp(
			FuelLerpElapsed /
				FuelLerpDuration,
			0.f,
			1.f)
		: 1.f;

	DisplayedFuel =
		FMath::Lerp(
			FuelLerpStartValue,
			CurrentServerFuel,
			Alpha);

	if (Alpha >= 1.f)
	{
		DisplayedFuel =
			CurrentServerFuel;

		SetComponentTickEnabled(false);
	}
}