#include "DRJetpackComponent.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"

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

	if (!IsValid(Character) ||
		!Character->HasAuthority() ||
		!bIsJetpackActive)
	{
		return;
	}

	UpdateFuel(DeltaTime);
}

void UDRJetpackComponent::RequestStart()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (!IsValid(Character) ||
		!Character->IsLocallyControlled())
	{
		return;
	}

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

	Character->HandleJetpackActiveStateChangedFromComponent();

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

	Character->HandleJetpackActiveStateChangedFromComponent();

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

	if (IsValid(Character))
	{
		Character->HandleJetpackRejectedByServer();
	}
}

void UDRJetpackComponent::OnRep_JetpackActive()
{
	ADRPlayerCharacter* Character =
		GetOwnerCharacter();

	if (IsValid(Character))
	{
		Character->HandleJetpackActiveStateChangedFromComponent();
	}
}