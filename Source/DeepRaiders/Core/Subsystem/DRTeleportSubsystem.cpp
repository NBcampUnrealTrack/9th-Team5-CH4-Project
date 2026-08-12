#include "DRTeleportSubsystem.h"

#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/teleport/DRTeleportPoint.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void UDRTeleportSubsystem::RegisterTeleportPoint(ADRTeleportPoint* TeleportPoint)
{
	if (!IsValid(TeleportPoint))
	{
		return;
	}

	TeleportPoints.AddUnique(TeleportPoint);
}

void UDRTeleportSubsystem::UnregisterTeleportPoint(ADRTeleportPoint* TeleportPoint)
{
	TeleportPoints.Remove(TeleportPoint);

	if (UWorld* World = GetWorld())
	{
		if (ADRMiningGameStateBase* GameState = World->GetGameState<ADRMiningGameStateBase>())
		{
			GameState->RemoveRegisteredTeleportPoint(TeleportPoint);
		}
	}
}

bool UDRTeleportSubsystem::TryRegisterTeleportPoint(ADRTeleportPoint* TeleportPoint, APawn* Interactor, int32 TeamId)
{
	if (!IsValid(TeleportPoint)
		|| !IsValid(Interactor)
		|| !Interactor->HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	ADRMiningGameStateBase* GameState = IsValid(World) ? World->GetGameState<ADRMiningGameStateBase>() : nullptr;
	if (!IsValid(GameState))
	{
		return false;
	}

	if (GameState->CanTeamUseRegisteredTeleportPoint(TeamId, TeleportPoint))
	{
		return false;
	}

	if (!TeleportPoint->TryRegisterForTeam(TeamId, Interactor))
	{
		return false;
	}

	GameState->AddTeamRegisteredTeleportPoint(TeamId, TeleportPoint);

	return true;
}

void UDRTeleportSubsystem::GetTeamRegisteredTeleportPoints(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();

	if (const UWorld* World = GetWorld())
	{
		if (const ADRMiningGameStateBase* GameState = World->GetGameState<ADRMiningGameStateBase>())
		{
			GameState->GetTeamRegisteredTeleportPoints(TeamId, OutTeleportPoints);
		}
	}
}

void UDRTeleportSubsystem::GetRegisteredTeleportPointsForTeam(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();

	if (const UWorld* World = GetWorld())
	{
		if (const ADRMiningGameStateBase* GameState = World->GetGameState<ADRMiningGameStateBase>())
		{
			GameState->GetRegisteredTeleportPointsForTeam(TeamId, OutTeleportPoints);
		}
	}
}

void UDRTeleportSubsystem::GetRegisteredTeleportDestinationsForTeam(int32 TeamId, ADRTeleportPoint* CurrentTeleportPoint, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();

	if (const UWorld* World = GetWorld())
	{
		if (const ADRMiningGameStateBase* GameState = World->GetGameState<ADRMiningGameStateBase>())
		{
			GameState->GetRegisteredTeleportDestinationsForTeam(TeamId, CurrentTeleportPoint, OutTeleportPoints);
		}
	}
}

bool UDRTeleportSubsystem::CanUseRegisteredTeleportPoint(int32 TeamId, ADRTeleportPoint* TeleportPoint) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ADRMiningGameStateBase* GameState = World->GetGameState<ADRMiningGameStateBase>())
		{
			return GameState->CanTeamUseRegisteredTeleportPoint(TeamId, TeleportPoint);
		}
	}

	return false;
}
