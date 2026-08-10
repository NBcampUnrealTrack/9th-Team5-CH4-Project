#include "DRVoxelInvokerControlComponent.h"

#include "DRVoxelInvokerActor.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UDRVoxelInvokerControlComponent::UDRVoxelInvokerControlComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    InvokerActorClass = ADRVoxelInvokerActor::StaticClass();
    RecentDigOrders.SetNumZeroed(RecentDigCount);
}

void UDRVoxelInvokerControlComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        SpawnInvokers();
    }
}

void UDRVoxelInvokerControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (ADRVoxelInvokerActor* Invoker : Invokers)
    {
        if (IsValid(Invoker))
        {
            Invoker->Destroy();
        }
    }

    Invokers.Reset();
    Super::EndPlay(EndPlayReason);
}

// 가장깊이 판 곳 인보크 트래킹
void UDRVoxelInvokerControlComponent::ReportDigLocation(const FVector& Location)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    UpdateRecentDigInvoker(Location);

    APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
    ADRPlayerState* PlayerState = PlayerController
        ? PlayerController->GetPlayerState<ADRPlayerState>()
        : nullptr;
    if (PlayerState && PlayerState->UpdateDeepestDigLocation(Location))
    {
        if (ADRVoxelInvokerActor* DeepestInvoker = GetInvoker(DeepestDigIndex))
        {
            DeepestInvoker->SetLocation(PlayerState->GetDeepestDigLocation());
        }
    }
}

// 최근 판 세 곳에 인보크 트래킹
void UDRVoxelInvokerControlComponent::UpdateRecentDigInvoker(const FVector& Location)
{
    int32 SelectedSlot = INDEX_NONE;
    float ClosestDistanceSquared = FMath::Square(RecentDigMergeDistance);

    for (int32 Slot = 0; Slot < RecentDigCount; ++Slot)
    {
        ADRVoxelInvokerActor* Invoker = GetInvoker(FirstRecentDigIndex + Slot);
        if (!Invoker || RecentDigOrders[Slot] == 0) continue;

        // 최근 판 곳이 Recent Invoker 액터 근처면 해당 액터를 이동
        const float DistanceSquared = FVector::DistSquared(Invoker->GetActorLocation(), Location);
        if (DistanceSquared <= ClosestDistanceSquared)
        {
            ClosestDistanceSquared = DistanceSquared;
            SelectedSlot = Slot;
        }
    }

    // 인보커 첫 사용
    if (SelectedSlot == INDEX_NONE)
    {
        for (int32 Slot = 0; Slot < RecentDigCount; ++Slot)
        {
            if (RecentDigOrders[Slot] == 0)
            {
                SelectedSlot = Slot;
                break;
            }
        }
    }

    // 가장 옛날 세팅된 인보커에 적용
    if (SelectedSlot == INDEX_NONE)
    {
        SelectedSlot = 0;
        for (int32 Slot = 1; Slot < RecentDigCount; ++Slot)
        {
            if (RecentDigOrders[Slot] < RecentDigOrders[SelectedSlot])
            {
                SelectedSlot = Slot;
            }
        }
    }

    // 인보커 위치 변동 우선순위 갱신
    if (ADRVoxelInvokerActor* Invoker = GetInvoker(FirstRecentDigIndex + SelectedSlot))
    {
        Invoker->SetLocation(Location);
        RecentDigOrders[SelectedSlot] = ++RecentDigSequence;
    }
}

// 가장 비싼 광물에 인보커 트래킹
void UDRVoxelInvokerControlComponent::TrackValuableOre(AActor* OreActor, int32 Value)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(OreActor)) return;

    ADRVoxelInvokerActor* Invoker = GetInvoker(ValuableOreIndex);
    if (!Invoker) return;

    if (!Invoker->HasTarget() || Value > HighestOreValue)
    {
        HighestOreValue = Value;
        Invoker->FollowActor(OreActor);
    }
}

// 폭탄 투하 시, 인보커 트래킹
void UDRVoxelInvokerControlComponent::TrackBomb(AActor* BombActor)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(BombActor)) return;

    ADRVoxelInvokerActor* Invoker = GetInvoker(NextBombIndex);
    if (Invoker)
    {
        Invoker->FollowActor(BombActor);
    }

    NextBombIndex = NextBombIndex + 1 < InvokerCount
        ? NextBombIndex + 1
        : FirstBombIndex;
}

// 폭발한 곳에 인보커 유지
void UDRVoxelInvokerControlComponent::HoldBombLocation(
    AActor* BombActor, const FVector& Location, float Duration)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    for (int32 Index = FirstBombIndex; Index < InvokerCount; ++Index)
    {
        ADRVoxelInvokerActor* Invoker = GetInvoker(Index);
        if (Invoker && Invoker->IsFollowing(BombActor))
        {
            Invoker->HoldLocation(Location, Duration);
            return;
        }
    }
}

void UDRVoxelInvokerControlComponent::SpawnInvokers()
{
    if (!InvokerActorClass) return;

    Invokers.Reserve(InvokerCount);
    for (int32 Index = 0; Index < InvokerCount; ++Index)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner();
        ADRVoxelInvokerActor* Invoker = GetWorld()->SpawnActor<ADRVoxelInvokerActor>(
            InvokerActorClass, FTransform::Identity, SpawnParams);
        if (Invoker)
        {
            Invoker->Configure(CollisionRange);
        }

        Invokers.Add(Invoker);
    }
}

ADRVoxelInvokerActor* UDRVoxelInvokerControlComponent::GetInvoker(int32 Index) const
{
    return Invokers.IsValidIndex(Index) ? Invokers[Index] : nullptr;
}
