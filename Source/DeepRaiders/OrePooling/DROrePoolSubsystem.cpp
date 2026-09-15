#include "DROrePoolSubsystem.h"

#include "DROreFieldActor.h"
#include "DROrePoolActor.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UDROrePoolSubsystem::QueuePrewarmOre(UDRItemDefinition* ItemDefinition,
    TSubclassOf<ADROrePoolActor> OreActorClass, int32 Count)
{
    if (!CanManagePool() || !IsValid(ItemDefinition) || !OreActorClass || Count <= 0) return;

    for (FDROrePrewarmRequest& Request : PrewarmRequests)
    {
        if (Request.ItemDefinition == ItemDefinition && Request.OreActorClass == OreActorClass)
        {
            Request.RemainingCount += Count;
            return;
        }
    }

    FDROrePrewarmRequest& Request = PrewarmRequests.AddDefaulted_GetRef();
    Request.ItemDefinition = ItemDefinition;
    Request.OreActorClass = OreActorClass;
    Request.RemainingCount = Count;
}

void UDROrePoolSubsystem::StartPrewarm(int32 InitialCount, int32 BatchSize)
{
    if (!CanManagePool() || PrewarmRequests.IsEmpty()) return;

    PrewarmBatchSize = FMath::Max(1, BatchSize);
    ProcessPrewarmBatch(FMath::Max(1, InitialCount));

    if (!PrewarmRequests.IsEmpty() && !bPrewarmScheduled)
    {
        bPrewarmScheduled = true;
        GetWorld()->GetTimerManager().SetTimerForNextTick(this,
            &ThisClass::ProcessPrewarmQueue);
    }
}

void UDROrePoolSubsystem::ProcessPrewarmQueue()
{
    bPrewarmScheduled = false;
    ProcessPrewarmBatch(PrewarmBatchSize);

    if (!PrewarmRequests.IsEmpty())
    {
        bPrewarmScheduled = true;
        GetWorld()->GetTimerManager().SetTimerForNextTick(this,
            &ThisClass::ProcessPrewarmQueue);
    }
}

void UDROrePoolSubsystem::ProcessPrewarmBatch(int32 Count)
{
    for (int32 Index = 0; Index < Count && !PrewarmRequests.IsEmpty(); ++Index)
    {
        FDROrePrewarmRequest& Request = PrewarmRequests[0];
        if (!IsValid(Request.ItemDefinition) || !Request.OreActorClass ||
            Request.RemainingCount <= 0)
        {
            PrewarmRequests.RemoveAt(0);
            --Index;
            continue;
        }

        ADROrePoolActor* OreActor = CreateOre(Request.ItemDefinition, Request.OreActorClass,
            FTransform::Identity, nullptr, INDEX_NONE);
        if (!OreActor)
        {
            PrewarmRequests.RemoveAt(0);
            --Index;
            continue;
        }

        Pools.FindOrAdd(Request.ItemDefinition).Actors.Add(OreActor);
        if (--Request.RemainingCount <= 0)
        {
            PrewarmRequests.RemoveAt(0);
        }
    }
}

ADROrePoolActor* UDROrePoolSubsystem::AcquireOre(
    UDRItemDefinition* ItemDefinition, TSubclassOf<ADROrePoolActor> OreActorClass,
    const FTransform& SpawnTransform, ADROreFieldActor* OwningField, int32 SpawnPointId)
{
    if (!CanManagePool() || !IsValid(ItemDefinition) || !OreActorClass || !IsValid(OwningField))
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
        return CreateOre(ItemDefinition, OreActorClass, SpawnTransform, OwningField,
            SpawnPointId);
    }

    const FTransform FinalTransform = ItemDefinition->WorldItemOffsetTransform * SpawnTransform;
    OreActor->ActivateFromPool(FinalTransform, OwningField, SpawnPointId);
    return OreActor;
}

ADROrePoolActor* UDROrePoolSubsystem::AcquireOre(
    UDRItemDefinition* ItemDefinition, TSubclassOf<ADROrePoolActor> OreActorClass,
    const FTransform& SpawnTransform, int32 SpawnPointId)
{
    if (!CanManagePool() || !IsValid(ItemDefinition) || !OreActorClass)
    {
        return nullptr;
    }

    FDROrePoolBucket& Pool = Pools.FindOrAdd(ItemDefinition);
    ADROrePoolActor* OreActor = nullptr;

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
        OreActor = CreateOre(ItemDefinition, OreActorClass, SpawnTransform, nullptr, INDEX_NONE);
    }

    if (!OreActor)
    {
        return nullptr;
    }

    const FTransform FinalTransform = ItemDefinition->WorldItemOffsetTransform * SpawnTransform;
    OreActor->ActivateFromPool(FinalTransform, nullptr, SpawnPointId);
    OreActor->MarkAsDropped();
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

    ADROreFieldActor* OwningField = OreActor->GetOwningField();
    if (IsValid(OwningField))
    {
        OwningField->HandleOreReleased(OreActor->GetSpawnPointId(), OreActor);
    }

    OreActor->DeactivateToPool();
    Pools.FindOrAdd(ItemDefinition).Actors.AddUnique(OreActor);
}

bool UDROrePoolSubsystem::CanManagePool() const
{
    const UWorld* World = GetWorld();
    return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

ADROrePoolActor* UDROrePoolSubsystem::CreateOre(
    UDRItemDefinition* ItemDefinition, TSubclassOf<ADROrePoolActor> OreActorClass,
    const FTransform& SpawnTransform, ADROreFieldActor* OwningField, int32 SpawnPointId)
{
    UWorld* World = GetWorld();
    const FTransform FinalTransform = ItemDefinition->WorldItemOffsetTransform * SpawnTransform;

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

    if (IsValid(OwningField))
    {
        OreActor->ActivateFromPool(FinalTransform, OwningField, SpawnPointId);
    }
    else
    {
        OreActor->DeactivateToPool();
    }

    UGameplayStatics::FinishSpawningActor(OreActor, FinalTransform);
    OreActor->ForceNetUpdate();
    return OreActor;
}
