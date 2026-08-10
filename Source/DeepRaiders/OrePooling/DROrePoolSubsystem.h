#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DROrePoolSubsystem.generated.h"

class ADROrePoolActor;
class ADROreFieldActor;
class UDRItemDefinition;

USTRUCT()
struct FDROrePrewarmRequest
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UDRItemDefinition> ItemDefinition;

    UPROPERTY()
    TSubclassOf<ADROrePoolActor> OreActorClass;

    int32 RemainingCount = 0;
};

USTRUCT()
struct FDROrePoolBucket
{
    GENERATED_BODY()

    // 현재 비활성 상태인 재사용 가능 광물
    UPROPERTY()
    TArray<TObjectPtr<ADROrePoolActor>> Actors;
};

// ItemDefinition별 광물 풀을 서버에서 관리한다.
UCLASS()
class DEEPRAIDERS_API UDROrePoolSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 필요한 광물 수량을 프리워밍 큐에 누적
    void QueuePrewarmOre(UDRItemDefinition* ItemDefinition,
        TSubclassOf<ADROrePoolActor> OreActorClass, int32 Count);

    // 첫 수량은 즉시 생성하고 나머지는 프레임 단위로 분할
    void StartPrewarm(int32 InitialCount, int32 BatchSize);

    // 같은 아이템과 클래스의 광물을 풀에서 활성화
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ore Pool",
        meta = (DeterminesOutputType = "OreActorClass"))
    ADROrePoolActor* AcquireOre(UDRItemDefinition* ItemDefinition,
        TSubclassOf<ADROrePoolActor> OreActorClass, const FTransform& SpawnTransform,
        ADROreFieldActor* OwningField, int32 SpawnPointId);

    // OreField 없이 드롭되는 광물 획득
    ADROrePoolActor* AcquireOre(UDRItemDefinition* ItemDefinition,
        TSubclassOf<ADROrePoolActor> OreActorClass, const FTransform& SpawnTransform,
        int32 SpawnPointId);

    // 활성 광물을 비활성화하고 풀에 반환
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ore Pool")
    void ReleaseOre(ADROrePoolActor* OreActor);

private:
    UPROPERTY()
    TMap<TObjectPtr<UDRItemDefinition>, FDROrePoolBucket> Pools;

    UPROPERTY()
    TArray<FDROrePrewarmRequest> PrewarmRequests;

    int32 PrewarmBatchSize = 100;
    bool bPrewarmScheduled = false;

    bool CanManagePool() const;
    void ProcessPrewarmQueue();
    void ProcessPrewarmBatch(int32 Count);
    ADROrePoolActor* CreateOre(UDRItemDefinition* ItemDefinition,
        TSubclassOf<ADROrePoolActor> OreActorClass, const FTransform& SpawnTransform,
        ADROreFieldActor* OwningField, int32 SpawnPointId);
};
