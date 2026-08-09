#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "DRMiningGameStateBase.generated.h"

UCLASS()
class DEEPRAIDERS_API ADRMiningGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	void RegisterTerrainDig(const FDRTerrainDigOperation& Operation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplyTerrainDig(const FDRTerrainDigOperation& Operation);

private:
	bool ApplyTerrainDigOnce(const FDRTerrainDigOperation& Operation);
};
