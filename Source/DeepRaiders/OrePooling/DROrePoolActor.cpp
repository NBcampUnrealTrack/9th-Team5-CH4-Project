#include "DROrePoolActor.h"

#include "DROreFieldActor.h"
#include "DROrePoolSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
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
    
    bInteractionInProgress = false;
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

    StaticMeshComponent->SetSimulatePhysics(true);

    if (bPoolActive)
    {
        OnActivatedFromPool();
    }
    else
    {
        OnDeactivatedToPool();
    }
}

bool ADROrePoolActor::CanInteract_Implementation(APawn* Interactor) const
{
    const ADRPlayerController* Controller = IsValid(Interactor) ? Cast<ADRPlayerController>(Interactor->GetController()) : nullptr;
    
    return HasAuthority() && bPoolActive && !bInteractionInProgress &&  ItemInstance.IsValid()
        && IsValid(Controller) && Controller->CanReceiveItem(ItemInstance.Definition, ItemInstance.Quantity);
}

bool ADROrePoolActor::Interact_Implementation(APawn* Interactor)
{
    if (!CanInteract_Implementation(Interactor))
    {
        return false;
    }
    
    ADRPlayerController* Controller = Cast<ADRPlayerController>(Interactor->GetController());
    bInteractionInProgress = true;
    
    if (!Controller->TryReceiveItem(ItemInstance.Definition, ItemInstance.Quantity))
    {
        bInteractionInProgress = false;
        return false;
    }
    
    // 광석은 반드시 OrePoolSubsystem을 통해서 반환
    UDROrePoolSubsystem* Pool = GetWorld()->GetSubsystem<UDROrePoolSubsystem>();
    
    if (IsValid(Pool))
    {
        Pool->ReleaseOre(this);
        return !IsPoolActive();
    }
        
    return false;
}
