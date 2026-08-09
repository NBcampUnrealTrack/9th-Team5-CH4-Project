#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DROrePoolSubsystem.generated.h"

class ADROrePoolActor;
class UDRItemDefinition;

USTRUCT()
struct FDROrePoolBucket
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<TObjectPtr<ADROrePoolActor>> Actors;
};

// ItemDefinition별 광물 풀을 서버에서 관리한다.
UCLASS()
class DEEPRAIDERS_API UDROrePoolSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ore Pool",
              meta = (DeterminesOutputType = "OreActorClass"))
    ADROrePoolActor* AcquireOre(UDRItemDefinition* ItemDefinition,
                                TSubclassOf<ADROrePoolActor> OreActorClass,
                                const FTransform& SpawnTransform, int32 SpawnPointId);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ore Pool")
    void ReleaseOre(ADROrePoolActor* OreActor);

private:
    UPROPERTY()
    TMap<TObjectPtr<UDRItemDefinition>, FDROrePoolBucket> Pools;

    bool CanManagePool() const;
    ADROrePoolActor* CreateOre(UDRItemDefinition* ItemDefinition,
                               TSubclassOf<ADROrePoolActor> OreActorClass,
                               const FTransform& SpawnTransform, int32 SpawnPointId);
};
