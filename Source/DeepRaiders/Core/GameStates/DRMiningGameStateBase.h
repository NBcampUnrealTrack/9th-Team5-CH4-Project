#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "GameFramework/GameStateBase.h"
#include "DRMiningGameStateBase.generated.h"

class ADRTeleportPoint;
class AVoxelWorld;
class FLifetimeProperty;

USTRUCT()
struct FDRTeamRegisteredTeleportPoint
{
	GENERATED_BODY()

	UPROPERTY()
	int32 TeamId = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<ADRTeleportPoint> TeleportPoint;
};

UCLASS()
class DEEPRAIDERS_API ADRMiningGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma region TerrainDig
public:
	void RegisterTerrainDig(const FDRTerrainDigOperation& Operation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplyTerrainDig(const FDRTerrainDigOperation& Operation);

private:
	bool ApplyTerrainDigOnce(const FDRTerrainDigOperation& Operation);
#pragma endregion 

#pragma region Snow
public:
	void RegisterSnowAdd(const FDRSnowAddOperation& Operation);
	void RegisterSnowRemove(const FDRSnowRemoveOperation& Operation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplySnowAdd(const FDRSnowAddOperation& Operation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplySnowRemove(const FDRSnowRemoveOperation& Operation);

private:
	bool ApplySnowAddOnce(const FDRSnowAddOperation& Operation);
	bool ApplySnowRemoveOnce(const FDRSnowRemoveOperation& Operation);
	AVoxelWorld* ResolveVoxelWorldByName(FName VoxelWorldName) const;
#pragma endregion
	
#pragma region Teleport
public:
	void AddTeamRegisteredTeleportPoint(int32 TeamId, ADRTeleportPoint* TeleportPoint);
	void RemoveRegisteredTeleportPoint(ADRTeleportPoint* TeleportPoint);

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetTeamRegisteredTeleportPoints(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetRegisteredTeleportPointsForTeam(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetRegisteredTeleportDestinationsForTeam(int32 TeamId, ADRTeleportPoint* CurrentTeleportPoint, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	bool CanTeamUseRegisteredTeleportPoint(int32 TeamId, const ADRTeleportPoint* TeleportPoint) const;

private:
	UPROPERTY(Replicated)
	TArray<FDRTeamRegisteredTeleportPoint> TeamRegisteredTeleports;
#pragma endregion
};
