#pragma once

#include "CoreMinimal.h"

class AActor;
class APawn;
class UWorld;

namespace DRCombatTeam
{
    DEEPRAIDERS_API int32 GetActorTeamId(const AActor* Actor);
    
    DEEPRAIDERS_API bool IsFriendlyTarget(int32 SourceTeamId, const AActor* TargetActor);
    
    DEEPRAIDERS_API void GetFriendlyPawns(const UWorld* World, int32 SourceTeamId, TArray<APawn*>& OutFriendlyPawns);
}