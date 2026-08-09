#pragma once

#include "CoreMinimal.h"
#include "DROreFieldDefinition.h"
#include "GameFramework/Actor.h"
#include "DROreFieldActor.generated.h"

class ADROrePoolActor;
class UBoxComponent;

USTRUCT()
struct FDROreSpawnPoint
{
    GENERATED_BODY()

    // ItemDefinition Offset 적용 전 Transform
    FTransform Transform;

    int32 SpawnPointId = INDEX_NONE;

    UPROPERTY()
    TObjectPtr<UDRItemDefinition> ItemDefinition;

    TSubclassOf<ADROrePoolActor> OreActorClass;

    UPROPERTY()
    TObjectPtr<ADROrePoolActor> ActiveActor;
};

USTRUCT()
struct FDROreRuntimeSector
{
    GENERATED_BODY()

    float StartDepth = 0.f;
    float EndDepth = 0.f;
    bool bActive = false;
    TArray<FDROreSpawnPoint> SpawnPoints;
};

// 위치를 선계산하고 최심도에 따라 섹터를 켠다.
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADROreFieldActor : public AActor
{
    GENERATED_BODY()

public:
    ADROreFieldActor();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ore Field")
    TObjectPtr<UBoxComponent> Bounds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Field")
    TObjectPtr<UDROreFieldDefinition> Definition;

private:
    TArray<FDROreRuntimeSector> RuntimeSectors;
    FTimerHandle UpdateTimer;
    float DeepestReachedDepth = 0.f;
    int32 NextSpawnPointId = 0;

    void BuildSpawnPoints();
    void UpdateActiveSectors();
    void SetSectorActive(FDROreRuntimeSector& Sector, bool bNewActive);
    float FindDeepestPlayerDepth() const;
    bool IsPlayerInsideField(const FVector& WorldLocation) const;
    FTransform MakeSpawnTransform(const FDROreDepthSector& Sector,
                                  FRandomStream& Random) const;
    const FDROreWeight* ChooseOre(const FDROreDepthSector& Sector,
                                  FRandomStream& Random) const;
    
public:
    bool HandleOreCollected(ADROrePoolActor* OreActor);
};
