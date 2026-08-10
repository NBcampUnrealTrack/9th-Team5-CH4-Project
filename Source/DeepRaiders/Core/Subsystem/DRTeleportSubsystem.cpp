#include "DRTeleportSubsystem.h"

#include "DeepRaiders/teleport/DRTeleportPoint.h"
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
}

bool UDRTeleportSubsystem::TryRegisterTeleportPoint(ADRTeleportPoint* TeleportPoint, APawn* Interactor, int32 TeamId)
{
	if (!IsValid(TeleportPoint)
		|| !IsValid(Interactor)
		|| !Interactor->HasAuthority())
	{
		return false;
	}

	return TeleportPoint->TryRegisterForTeam(TeamId, Interactor);
}
