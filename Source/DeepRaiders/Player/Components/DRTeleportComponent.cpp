#include "DRTeleportComponent.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Core/Subsystem/DRTeleportSubsystem.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/teleport/DRTeleportPoint.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UDRTeleportComponent::UDRTeleportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRTeleportComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDRTeleportComponent, CurrentInteractableTeleport, COND_OwnerOnly);
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

void UDRTeleportComponent::RequestTeleportTo(ADRTeleportPoint* DestinationTeleportPoint)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled() || !IsValid(DestinationTeleportPoint))
	{
		return;
	}

	ServerRequestTeleportTo(DestinationTeleportPoint);
}

bool UDRTeleportComponent::IsTeleportPointRegistered(ADRTeleportPoint* TeleportPoint) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ADRMiningGameStateBase* GameState = World->GetGameState<ADRMiningGameStateBase>())
		{
			return GameState->CanTeamUseRegisteredTeleportPoint(GetOwnerTeamId(), TeleportPoint);
		}
	}

	return false;
}

void UDRTeleportComponent::GetRegisteredTeleportPoints(TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();

	if (const UWorld* World = GetWorld())
	{
		if (const ADRMiningGameStateBase* GameState = World->GetGameState<ADRMiningGameStateBase>())
		{
			GameState->GetRegisteredTeleportPointsForTeam(GetOwnerTeamId(), OutTeleportPoints);
		}
	}
}

void UDRTeleportComponent::GetRegisteredTeleportDestinations(ADRTeleportPoint* CurrentTeleportPoint, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	GetRegisteredTeleportPoints(OutTeleportPoints);
	OutTeleportPoints.Remove(CurrentTeleportPoint);
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

int32 UDRTeleportComponent::GetOwnerTeamId() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const ADRPlayerState* DRPlayerState = IsValid(OwnerPawn) ? OwnerPawn->GetPlayerState<ADRPlayerState>() : nullptr;
	return IsValid(DRPlayerState) ? DRPlayerState->GetTeamId() : INDEX_NONE;
}

void UDRTeleportComponent::ServerRequestRegisterTeleport_Implementation(ADRTeleportPoint* TeleportPoint)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	ADRTeleportPoint* TargetTeleportPoint = IsValid(TeleportPoint) ? TeleportPoint : nullptr;
	const int32 TeamId = GetOwnerTeamId();

	if (!IsValid(OwnerPawn) || !IsValid(TargetTeleportPoint) || TeamId == INDEX_NONE)
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

	TeleportSubsystem->TryRegisterTeleportPoint(TargetTeleportPoint, OwnerPawn, TeamId);
}

void UDRTeleportComponent::ServerRequestTeleportTo_Implementation(ADRTeleportPoint* DestinationTeleportPoint)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !IsValid(DestinationTeleportPoint) || DestinationTeleportPoint == CurrentInteractableTeleport)
	{
		return;
	}

	const ADRPlayerCharacter* PlayerCharacter = Cast<ADRPlayerCharacter>(OwnerPawn);
	if (IsValid(PlayerCharacter) && PlayerCharacter->IsDead())
	{
		return;
	}

	if (!IsTeleportPointRegistered(DestinationTeleportPoint))
	{
		return;
	}

	const FTransform ArrivalTransform = DestinationTeleportPoint->GetTeleportArrivalTransform();
	OwnerPawn->TeleportTo(ArrivalTransform.GetLocation(), ArrivalTransform.Rotator(), false, true);
}
