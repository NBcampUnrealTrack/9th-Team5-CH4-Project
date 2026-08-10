#include "DRTeleportComponent.h"

#include "DeepRaiders/Core/Subsystem/DRTeleportSubsystem.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/teleport/DRTeleportPoint.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UDRTeleportComponent::UDRTeleportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRTeleportComponent::RequestRegisterCurrentTeleport()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	ServerRequestRegisterTeleport(CurrentInteractableTeleport);
}

void UDRTeleportComponent::SetCurrentInteractableTeleport(ADRTeleportPoint* TeleportPoint)
{
	if (!IsValid(TeleportPoint))
	{
		return;
	}

	// Keep the most recent teleport volume as the current interaction target.
	CurrentInteractableTeleport = TeleportPoint;
}

void UDRTeleportComponent::ClearCurrentInteractableTeleport(ADRTeleportPoint* TeleportPoint)
{
	if (CurrentInteractableTeleport != TeleportPoint)
	{
		return;
	}

	CurrentInteractableTeleport = nullptr;
}

void UDRTeleportComponent::ServerRequestRegisterTeleport_Implementation(ADRTeleportPoint* TeleportPoint)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	ADRTeleportPoint* TargetTeleportPoint = IsValid(TeleportPoint) ? TeleportPoint : nullptr;

	if (!IsValid(OwnerPawn) || !IsValid(TargetTeleportPoint))
	{
		return;
	}

	const ADRPlayerCharacter* PlayerCharacter = Cast<ADRPlayerCharacter>(OwnerPawn);
	if (IsValid(PlayerCharacter) && PlayerCharacter->IsDead())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UDRTeleportSubsystem* TeleportSubsystem = World->GetSubsystem<UDRTeleportSubsystem>();
	if (!IsValid(TeleportSubsystem))
	{
		return;
	}

	// Team system is not wired yet, so TeamId 0 verifies only the registration path.
	TeleportSubsystem->TryRegisterTeleportPoint(TargetTeleportPoint, OwnerPawn, GetTemporaryTeamId());
}
