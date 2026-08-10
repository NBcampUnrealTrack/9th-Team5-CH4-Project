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

	if (TeleportPoint->IsRegistered())
	{
		AddRegisteredTeleportPoint(TeleportPoint, TeleportPoint->GetOwnerTeamId());
	}
}

void UDRTeleportSubsystem::UnregisterTeleportPoint(ADRTeleportPoint* TeleportPoint)
{
	TeleportPoints.Remove(TeleportPoint);
	RemoveRegisteredTeleportPoint(TeleportPoint);
}

bool UDRTeleportSubsystem::TryRegisterTeleportPoint(ADRTeleportPoint* TeleportPoint, APawn* Interactor, int32 TeamId)
{
	if (!IsValid(TeleportPoint)
		|| !IsValid(Interactor)
		|| !Interactor->HasAuthority())
	{
		return false;
	}

	if (!TeleportPoint->TryRegisterForTeam(TeamId, Interactor))
	{
		return false;
	}

	AddRegisteredTeleportPoint(TeleportPoint, TeamId);
	return true;
}

void UDRTeleportSubsystem::GetPublicRegisteredTeleportPoints(TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();
	AppendValidTeleportPoints(PublicRegisteredTeleports, OutTeleportPoints);
}

void UDRTeleportSubsystem::GetTeamRegisteredTeleportPoints(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();

	if (const FDRTeleportPointList* TeamTeleportPoints = TeamRegisteredTeleports.Find(TeamId))
	{
		AppendValidTeleportPoints(TeamTeleportPoints->TeleportPoints, OutTeleportPoints);
	}
}

void UDRTeleportSubsystem::GetRegisteredTeleportPointsForTeam(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	OutTeleportPoints.Reset();
	AppendValidTeleportPoints(PublicRegisteredTeleports, OutTeleportPoints);

	if (const FDRTeleportPointList* TeamTeleportPoints = TeamRegisteredTeleports.Find(TeamId))
	{
		AppendValidTeleportPoints(TeamTeleportPoints->TeleportPoints, OutTeleportPoints);
	}
}

void UDRTeleportSubsystem::GetRegisteredTeleportDestinationsForTeam(int32 TeamId, ADRTeleportPoint* CurrentTeleportPoint, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	GetRegisteredTeleportPointsForTeam(TeamId, OutTeleportPoints);
	OutTeleportPoints.Remove(CurrentTeleportPoint);
}

bool UDRTeleportSubsystem::CanUseRegisteredTeleportPoint(int32 TeamId, ADRTeleportPoint* TeleportPoint) const
{
	if (!IsValid(TeleportPoint) || !TeleportPoint->IsRegistered())
	{
		return false;
	}

	if (PublicRegisteredTeleports.Contains(TeleportPoint))
	{
		return true;
	}

	const FDRTeleportPointList* TeamTeleportPoints = TeamRegisteredTeleports.Find(TeamId);
	return TeamTeleportPoints && TeamTeleportPoints->TeleportPoints.Contains(TeleportPoint);
}

void UDRTeleportSubsystem::AddRegisteredTeleportPoint(ADRTeleportPoint* TeleportPoint, int32 TeamId)
{
	if (!IsValid(TeleportPoint))
	{
		return;
	}

	if (TeleportPoint->GetAccessType() == EDRTeleportAccessType::Public)
	{
		PublicRegisteredTeleports.AddUnique(TeleportPoint);
		return;
	}

	TeamRegisteredTeleports.FindOrAdd(TeamId).TeleportPoints.AddUnique(TeleportPoint);
}

void UDRTeleportSubsystem::RemoveRegisteredTeleportPoint(ADRTeleportPoint* TeleportPoint)
{
	PublicRegisteredTeleports.Remove(TeleportPoint);

	for (TPair<int32, FDRTeleportPointList>& TeamTeleportPair : TeamRegisteredTeleports)
	{
		TeamTeleportPair.Value.TeleportPoints.Remove(TeleportPoint);
	}
}

void UDRTeleportSubsystem::AppendValidTeleportPoints(const TArray<TObjectPtr<ADRTeleportPoint>>& Source, TArray<ADRTeleportPoint*>& OutTeleportPoints) const
{
	for (ADRTeleportPoint* TeleportPoint : Source)
	{
		if (IsValid(TeleportPoint))
		{
			OutTeleportPoints.AddUnique(TeleportPoint);
		}
	}
}
