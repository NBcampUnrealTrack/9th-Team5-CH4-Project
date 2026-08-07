#include "DROrePoolActor.h"

#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ADROrePoolActor::ADROrePoolActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);
}

void ADROrePoolActor::BeginPlay()
{
    Super::BeginPlay();
    ApplyPoolState();
}

void ADROrePoolActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ThisClass, bPoolActive);
    DOREPLIFETIME(ThisClass, SpawnPointId);
}

void ADROrePoolActor::ActivateFromPool(const FTransform& SpawnTransform,
                                       int32 InSpawnPointId)
{
    if (!HasAuthority())
    {
        return;
    }

    // 재사용 전 Dormancy를 깨워 변경값을 복제한다.
    SetNetDormancy(DORM_Awake);
    SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

    SpawnPointId = InSpawnPointId;
    bPoolActive = true;

    // Deferred Spawn 중에는 BeginPlay에서 적용한다.
    if (HasActorBegunPlay())
    {
        ApplyPoolState();
        ForceNetUpdate();
    }
}

void ADROrePoolActor::DeactivateToPool()
{
    if (!HasAuthority() || !bPoolActive)
    {
        return;
    }

    bPoolActive = false;
    SpawnPointId = INDEX_NONE;
    ApplyPoolState();
    ForceNetUpdate();
}

bool ADROrePoolActor::IsPoolActive() const
{
    return bPoolActive;
}

int32 ADROrePoolActor::GetSpawnPointId() const
{
    return SpawnPointId;
}

void ADROrePoolActor::OnRep_PoolState()
{
    ApplyPoolState();
}

void ADROrePoolActor::ApplyPoolState()
{
    SetActorHiddenInGame(!bPoolActive);
    SetActorEnableCollision(bPoolActive);
    SetActorTickEnabled(bPoolActive);

    StaticMeshComponent->SetSimulatePhysics(false);

    if (bPoolActive)
    {
        OnActivatedFromPool();
    }
    else
    {
        OnDeactivatedToPool();
    }
}
