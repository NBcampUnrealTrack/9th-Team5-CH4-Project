#include "DROrePoolActor.h"

#include "DROreFieldActor.h"
#include "DROrePoolSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

ADROrePoolActor::ADROrePoolActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = false;
    SetReplicateMovement(true);
    SetWorldState(EDROreWorldState::Dropped);
    StaticMeshComponent->SetUseCCD(true);
}

void ADROrePoolActor::BeginPlay()
{
    Super::BeginPlay();
    SetWorldState(WorldState);
    ApplyPoolState();
}

void ADROrePoolActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ThisClass, bPoolActive);
    DOREPLIFETIME(ThisClass, SpawnPointId);
    DOREPLIFETIME(ThisClass, WorldState);
}

bool ADROrePoolActor::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
    const FVector& SrcLocation) const
{
    // 풀링 된 광물은 복제 X
    if (WorldState == EDROreWorldState::Pooled)
    {
        return false;
    }

    // 거리별 네트워킹
    return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void ADROrePoolActor::ActivateFromPool(const FTransform& SpawnTransform, ADROreFieldActor* InOwningField, int32 InSpawnPointId)
{
    if (!HasAuthority())
    {
        return;
    }
    
    ResetInteractionState();
    
    // 재사용 전 Dormancy를 깨워 변경값을 복제한다.
    SetNetDormancy(DORM_Awake);
    SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

    OwningField = InOwningField;
    SpawnPointId = InSpawnPointId;
    bPoolActive = true;
    GetWorldTimerManager().ClearTimer(GroundCheckTimer);
    GetWorldTimerManager().ClearTimer(DetachedTimeoutTimer);
    bHasLastGroundLocation = false;
    SetWorldState(EDROreWorldState::Embedded);

    // Deferred Spawn 중이면 BeginPlay에서 상태 적용
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
    OwningField.Reset();
    GetWorldTimerManager().ClearTimer(GroundCheckTimer);
    GetWorldTimerManager().ClearTimer(DetachedTimeoutTimer);
    SetWorldState(EDROreWorldState::Pooled);
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

EDROreWorldState ADROrePoolActor::GetWorldState() const
{
    return WorldState;
}

void ADROrePoolActor::MarkAsDropped()
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(GroundCheckTimer);
    GetWorldTimerManager().ClearTimer(DetachedTimeoutTimer);

    if (OwningField.IsValid())
    {
        OwningField->HandleOreReleased(SpawnPointId, this);
    }

    OwningField.Reset();
    SpawnPointId = INDEX_NONE;
    bPoolActive = true;
    SetWorldState(EDROreWorldState::Dropped);
    ApplyPoolState();
    BroadcastDropped();
    ForceNetUpdate();
}

void ADROrePoolActor::ScheduleGroundCheck(const FVector& DigLocation)
{
    if (!HasAuthority() || WorldState != EDROreWorldState::Embedded) return;

    LastDigLocation = DigLocation;
    GetWorldTimerManager().SetTimer(GroundCheckTimer, this,
        &ThisClass::MarkAsDetached, GroundCheckDelay, false);
}

void ADROrePoolActor::MarkAsDetached()
{
    if (!HasAuthority() || WorldState != EDROreWorldState::Embedded) return;

    if (OwningField.IsValid())
    {
        OwningField->HandleOreReleased(SpawnPointId, this);
    }

    // 추락 시작
    OwningField.Reset();
    SpawnPointId = INDEX_NONE;
    bPoolActive = true;
    DetachedStartLocation = GetActorLocation(); // 첫 위치
    bHasLastGroundLocation = false; // 안전 위치 없음

    // 파인 위치로 융기
    const FVector MoveDirection = (LastDigLocation - DetachedStartLocation).GetSafeNormal();
    const FVector DetachedLocation = DetachedStartLocation + MoveDirection * StaticMeshComponent->Bounds.SphereRadius;
    SetActorLocation(DetachedLocation, false, nullptr, ETeleportType::TeleportPhysics);

    ArmGroundHitEvent();
    SetWorldState(EDROreWorldState::Detached);
    ApplyPoolState();
    BroadcastMined();
    ForceNetUpdate();

    // 땅 위치 체크
    GetWorldTimerManager().SetTimer(GroundCheckTimer, this,
        &ThisClass::CheckGroundBelow, GroundCheckDelay, true);
    // 추락 타임아웃
    GetWorldTimerManager().SetTimer(DetachedTimeoutTimer, this,
        &ThisClass::HandleDetachedTimeout, DetachedTimeout, false);
}

void ADROrePoolActor::HandleDetachedTimeout()
{
    if (!HasAuthority() || WorldState != EDROreWorldState::Detached) return;

    if (bHasLastGroundLocation)
    {
        SetActorLocation(LastGroundLocation, false, nullptr, ETeleportType::TeleportPhysics);
        MarkAsDropped();
        return;
    }

    GetWorldTimerManager().ClearTimer(GroundCheckTimer);
    SetActorLocation(DetachedStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
    SetWorldState(EDROreWorldState::Embedded);
    ApplyPoolState();
    ForceNetUpdate();
}

void ADROrePoolActor::CheckGroundBelow()
{
    if (!HasAuthority() || WorldState != EDROreWorldState::Detached) return;

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OreGroundCheck), false, this);
    const FBoxSphereBounds& MeshBounds = StaticMeshComponent->Bounds;

    const float SphereRadius = FMath::Max(5.f, FMath::Min(MeshBounds.BoxExtent.X, MeshBounds.BoxExtent.Y) * 0.5f);
    const FVector Start = MeshBounds.Origin;
    const FVector End = Start - FVector::UpVector * GroundCheckDistance;
    const FCollisionShape Sphere = FCollisionShape::MakeSphere(SphereRadius);
    if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, Sphere, Params)) return;

    AActor* HitOwner = Hit.GetComponent() ? Hit.GetComponent()->GetOwner() : Hit.GetActor();
    if (Cast<AVoxelWorld>(Hit.GetActor()) || Cast<AVoxelWorld>(HitOwner))
    {
        const float BottomOffset = GetActorLocation().Z - (MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z);
        FVector SafeLocation = GetActorLocation();
        SafeLocation.Z = Hit.ImpactPoint.Z + BottomOffset + 2.f;
        LastGroundLocation = SafeLocation;
        bHasLastGroundLocation = true;

        const float DistanceToGround = Start.Z - Hit.ImpactPoint.Z;
        if (DistanceToGround > MeshBounds.BoxExtent.Z + SphereRadius) return;

        SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);
        MarkAsDropped();
    }
}

ADROreFieldActor* ADROrePoolActor::GetOwningField() const
{
    return OwningField.Get();
}

void ADROrePoolActor::OnRep_PoolState()
{
    ApplyPoolState();
}

void ADROrePoolActor::SetWorldState(EDROreWorldState NewState)
{
    WorldState = NewState;

    const bool bIsLooseOre = WorldState == EDROreWorldState::Detached ||
        WorldState == EDROreWorldState::Dropped;
    const float CullDistance = bIsLooseOre
        ? DroppedNetCullDistance
        : EmbeddedNetCullDistance;
    SetNetCullDistanceSquared(FMath::Square(CullDistance));
}

void ADROrePoolActor::ApplyPoolState()
{
    SetActorHiddenInGame(!bPoolActive);
    SetActorEnableCollision(bPoolActive);
    SetActorTickEnabled(bPoolActive);
    const bool bSimulatePhysics = WorldState == EDROreWorldState::Detached ||
        WorldState == EDROreWorldState::Dropped;
    StaticMeshComponent->SetSimulatePhysics(bSimulatePhysics);

    if (bPoolActive)
    {
        if (bSimulatePhysics)
        {
            StaticMeshComponent->WakeAllRigidBodies();
        }

        OnActivatedFromPool();
    }
    else
    {
        OnDeactivatedToPool();
    }
}

bool ADROrePoolActor::IsPickupAvailable() const
{
    const UWorld* World = GetWorld();
    
    return bPoolActive && IsValid(World) && IsValid(World->GetSubsystem<UDROrePoolSubsystem>());
}

bool ADROrePoolActor::FinalizePickup()
{
    UDROrePoolSubsystem* Pool = GetWorld()->GetSubsystem<UDROrePoolSubsystem>();
    
    if (!IsValid(Pool))
    {
        return false;
    }
    
    Pool->ReleaseOre(this);
    return !IsPoolActive();
}
