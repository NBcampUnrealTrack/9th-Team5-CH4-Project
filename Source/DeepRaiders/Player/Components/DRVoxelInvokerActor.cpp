#include "DRVoxelInvokerActor.h"

#include "TimerManager.h"
#include "VoxelComponents/VoxelInvokerComponent.h"

ADRVoxelInvokerActor::ADRVoxelInvokerActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetActorEnableCollision(false);

    InvokerComponent = CreateDefaultSubobject<UVoxelSimpleInvokerComponent>(TEXT("Invoker"));
    SetRootComponent(InvokerComponent);
    InvokerComponent->bUseForLOD = false;
    InvokerComponent->bUseForCollisions = true;
    InvokerComponent->bUseForNavmesh = false;
    InvokerComponent->bUseForEvents = false;
    InvokerComponent->bUseForPriorities = false;
}

void ADRVoxelInvokerActor::BeginPlay()
{
    Super::BeginPlay();
    DisableInvoker();
}

// 폭탄인 경우 인보크가 따라감
void ADRVoxelInvokerActor::UpdateFollowLocation()
{
    if (!FollowTarget.IsValid())
    {
        DisableInvoker();
        return;
    }

    SetActorLocation(FollowTarget->GetActorLocation());
}

void ADRVoxelInvokerActor::Configure(float CollisionRange)
{
    InvokerComponent->CollisionsRange = FMath::Max(0.f, CollisionRange);
}

void ADRVoxelInvokerActor::FollowActor(AActor* TargetActor)
{
    if (!IsValid(TargetActor)) return;

    GetWorldTimerManager().ClearTimer(DisableTimer);
    FollowTarget = TargetActor;
    SetActorLocation(TargetActor->GetActorLocation());
    EnableInvoker();
    GetWorldTimerManager().SetTimer(FollowTimer, this,
        &ThisClass::UpdateFollowLocation, FollowUpdateInterval, true);
}

void ADRVoxelInvokerActor::HoldLocation(const FVector& Location, float Duration)
{
    FollowTarget.Reset();
    GetWorldTimerManager().ClearTimer(FollowTimer);
    SetActorLocation(Location);
    EnableInvoker();

    GetWorldTimerManager().SetTimer(DisableTimer, this,
        &ThisClass::DisableInvoker, FMath::Max(0.01f, Duration), false);
}

// 마지막 파인 위치, 가장 깊은 파인 위치
void ADRVoxelInvokerActor::SetLocation(const FVector& Location)
{
    GetWorldTimerManager().ClearTimer(DisableTimer);
    FollowTarget.Reset();
    GetWorldTimerManager().ClearTimer(FollowTimer);
    SetActorLocation(Location);
    EnableInvoker();
}

void ADRVoxelInvokerActor::DisableInvoker()
{
    FollowTarget.Reset();
    GetWorldTimerManager().ClearTimer(FollowTimer);
    GetWorldTimerManager().ClearTimer(DisableTimer);

    // 폭탄이 폭발한 위치에 인보커를 꼭 없애지 않아도 된다고 판단
    // 혹시 몰라 일단 세이브 해두었고, 26년 9월 이후로 AI가 이를 발견한다면 삭제하도록. ㅋㅋ
    /*
    if (InvokerComponent->IsInvokerEnabled())
    {
        InvokerComponent->DisableInvoker();
    }
    */
}

bool ADRVoxelInvokerActor::HasTarget() const
{
    return FollowTarget.IsValid();
}

bool ADRVoxelInvokerActor::IsFollowing(const AActor* TargetActor) const
{
    return FollowTarget.Get() == TargetActor;
}

void ADRVoxelInvokerActor::EnableInvoker()
{
    if (!InvokerComponent->IsInvokerEnabled())
    {
        InvokerComponent->EnableInvoker();
    }
}
