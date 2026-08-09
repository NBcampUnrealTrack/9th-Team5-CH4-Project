#include "DROrePoolSubsystem.h"

#include "DROrePoolActor.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ADROrePoolActor* UDROrePoolSubsystem::AcquireOre(UDRItemDefinition* ItemDefinition,
    TSubclassOf<ADROrePoolActor> OreActorClass,
    const FTransform& SpawnTransform, int32 SpawnPointId)
{
    if (!CanManagePool() || !IsValid(ItemDefinition) || !OreActorClass)
    {
        return nullptr;
    }

    FDROrePoolBucket& Pool = Pools.FindOrAdd(ItemDefinition);
    ADROrePoolActor* OreActor = nullptr;

    // Definition과 Actor Class가 모두 같은 것만 재사용한다.
    for (int32 Index = Pool.Actors.Num() - 1; Index >= 0; --Index)
    {
        ADROrePoolActor* Candidate = Pool.Actors[Index];
        if (!IsValid(Candidate))
        {
            Pool.Actors.RemoveAtSwap(Index);
            continue;
        }

        if (Candidate->GetClass() == OreActorClass)
        {
            OreActor = Candidate;
            Pool.Actors.RemoveAtSwap(Index);
            break;
        }
    }

    if (!OreActor)
    {
        return CreateOre(ItemDefinition, OreActorClass, SpawnTransform, SpawnPointId);
    }

    const FTransform FinalTransform = ItemDefinition->SpawnOffsetTransform * SpawnTransform;
    OreActor->ActivateFromPool(FinalTransform, SpawnPointId);
    return OreActor;
}

void UDROrePoolSubsystem::ReleaseOre(ADROrePoolActor* OreActor)
{
    if (!CanManagePool() || !IsValid(OreActor) || !OreActor->IsPoolActive())
    {
        return;
    }

    UDRItemDefinition* ItemDefinition = OreActor->GetItemInstance().Definition;
    if (!IsValid(ItemDefinition))
    {
        return;
    }

    OreActor->DeactivateToPool();
    Pools.FindOrAdd(ItemDefinition).Actors.AddUnique(OreActor);
}

bool UDROrePoolSubsystem::CanManagePool() const
{
    const UWorld* World = GetWorld();
    return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

ADROrePoolActor* UDROrePoolSubsystem::CreateOre(UDRItemDefinition* ItemDefinition,
    TSubclassOf<ADROrePoolActor> OreActorClass,
    const FTransform& SpawnTransform, int32 SpawnPointId)
{
    UWorld* World = GetWorld();
    const FTransform FinalTransform = ItemDefinition->SpawnOffsetTransform * SpawnTransform;

    ADROrePoolActor* OreActor = World->SpawnActorDeferred<ADROrePoolActor>(
        OreActorClass, FinalTransform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!OreActor)
    {
        return nullptr;
    }

    FDRItemInstance ItemInstance;
    ItemInstance.Definition = ItemDefinition;
    ItemInstance.InstanceId = FGuid::NewGuid();
    ItemInstance.Quantity = 1;

    // 부모가 BeginPlay에서 검사하므로 먼저 설정한다.
    if (!OreActor->SetInitialItemInstance(ItemInstance))
    {
        OreActor->Destroy();
        return nullptr;
    }

    OreActor->ActivateFromPool(FinalTransform, SpawnPointId);
    UGameplayStatics::FinishSpawningActor(OreActor, FinalTransform);
    OreActor->ForceNetUpdate();
    return OreActor;
}
