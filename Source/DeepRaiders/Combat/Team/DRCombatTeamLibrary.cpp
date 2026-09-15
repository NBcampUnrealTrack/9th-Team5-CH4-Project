
#include "DRCombatTeamLibrary.h"

#include "DeepRaiders/Core/Interface/DRCombatTeamInterface.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

const ADRPlayerState* ResolvePlayerState(const AActor* Actor)
{
	if (const ADRPlayerState* PlayerState = Cast<ADRPlayerState>(Actor))
	{
		return PlayerState;
	}
	
	const APawn* Pawn = Cast<APawn>(Actor);
	return IsValid(Pawn) ? Pawn->GetPlayerState<ADRPlayerState>() : nullptr;
}

DEEPRAIDERS_API int32 DRCombatTeam::GetActorTeamId(const AActor* Actor)
{
	if (const IDRCombatTeamInterface* TeamActor = Cast<IDRCombatTeamInterface>(Actor))
	{
		return TeamActor->GetCombatTeamId();
	}

	const ADRPlayerState* PlayerState = ResolvePlayerState(Actor);
	return IsValid(PlayerState) ? PlayerState->GetTeamId() : INDEX_NONE;
}

DEEPRAIDERS_API bool DRCombatTeam::IsFriendlyTarget(int32 SourceTeamId, const AActor* TargetActor)
{
	if (SourceTeamId == INDEX_NONE 
		|| !IsValid(TargetActor))
	{
		return false;
	}
	
	const int32 TargetTeamId = GetActorTeamId(TargetActor);
	return TargetTeamId != INDEX_NONE && TargetTeamId == SourceTeamId;
}

DEEPRAIDERS_API void DRCombatTeam::GetFriendlyPawns(const UWorld* World, int32 SourceTeamId,
                                                    TArray<APawn*>& OutFriendlyPawns)
{
	OutFriendlyPawns.Reset();
	
	if (!IsValid(World)
		|| SourceTeamId == INDEX_NONE)
	{
		return;
	}

	const AGameStateBase* GameState = World->GetGameState();
	if (!IsValid(GameState))
	{
		return;
	}
	
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const ADRPlayerState* DRPlayerState = Cast<ADRPlayerState>(PlayerState);
		
		if (!IsValid(PlayerState)
			|| DRPlayerState->GetTeamId() != SourceTeamId)
		{
			continue;
		}
		
		APawn* Pawn = DRPlayerState->GetPawn();
		if (IsValid(Pawn))
		{
			OutFriendlyPawns.Add(Pawn);
		}
	}
}
