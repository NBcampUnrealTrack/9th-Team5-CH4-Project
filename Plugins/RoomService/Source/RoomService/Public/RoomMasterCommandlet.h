#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "RoomMasterCommandlet.generated.h"

// UnrealEditor-Cmd <project> -run=RoomMaster -unattended -nullrhi
UCLASS()
class ROOMSERVICE_API URoomMasterCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URoomMasterCommandlet();
	virtual int32 Main(const FString& Params) override;
};
