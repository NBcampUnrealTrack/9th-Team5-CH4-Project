#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DRCombatTeamInterface.generated.h"

UINTERFACE(MinimalAPI)
class UDRCombatTeamInterface : public UInterface
{
	GENERATED_BODY()
};

/** PlayerState가 없는 월드 오브젝트가 팀 판정에 참여할 때 구현한다. */
class DEEPRAIDERS_API IDRCombatTeamInterface
{
	GENERATED_BODY()

public:
	virtual int32 GetCombatTeamId() const = 0;
};
