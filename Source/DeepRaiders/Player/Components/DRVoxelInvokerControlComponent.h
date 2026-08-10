#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRVoxelInvokerControlComponent.generated.h"

class ADRVoxelInvokerActor;

// 플레이어가 사용하는 고정 수량의 Voxel Invoker 관리
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Voxel), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRVoxelInvokerControlComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDRVoxelInvokerControlComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 땅 팠을 때 Invoker 이동
    void ReportDigLocation(const FVector& Location);

    // 가장 값비싼 광물 위치로 Invoker 이동
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Voxel Invoker")
    void TrackValuableOre(AActor* OreActor, int32 Value);

    // 폭탄에 Invoker 이동
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Voxel Invoker")
    void TrackBomb(AActor* BombActor);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Voxel Invoker")
    void HoldBombLocation(AActor* BombActor, const FVector& Location, float Duration = 5.f);

protected:
    // 슬롯에 생성할 InvokerActor 또는 BP 자식 클래스
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel Invoker")
    TSubclassOf<ADRVoxelInvokerActor> InvokerActorClass;

    UPROPERTY(EditDefaultsOnly, Category = "Voxel Invoker", meta = (ClampMin = "0.0"))
    float CollisionRange = 5000.f;

    // 가까운 최근 채굴 위치를 같은 Invoker로 묶을 거리
    UPROPERTY(EditDefaultsOnly, Category = "Voxel Invoker", meta = (ClampMin = "0.0"))
    float RecentDigMergeDistance = 2500.f;

private:
    static constexpr int32 InvokerCount = 10;
    static constexpr int32 DeepestDigIndex = 0;
    static constexpr int32 FirstRecentDigIndex = 1;
    static constexpr int32 RecentDigCount = 3;
    static constexpr int32 ValuableOreIndex = 4;
    static constexpr int32 FirstBombIndex = 5;

    UPROPERTY()
    TArray<TObjectPtr<ADRVoxelInvokerActor>> Invokers;

    int32 HighestOreValue = MIN_int32;
    int32 NextBombIndex = FirstBombIndex;
    int64 RecentDigSequence = 0;
    TArray<int64> RecentDigOrders;

    void SpawnInvokers();
    void UpdateRecentDigInvoker(const FVector& Location);
    ADRVoxelInvokerActor* GetInvoker(int32 Index) const;
};
