#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DROrePoolActor.generated.h"

class FLifetimeProperty;
class ADROreFieldActor;

UENUM(BlueprintType)
enum class EDROreWorldState : uint8
{
    Pooled,
    Embedded,
    Detached,
    Dropped
};

// 서버에서 풀링되며 활성 상태가 복제된다.
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADROrePoolActor : public ADRWorldItemActor
{
    GENERATED_BODY()

public:
    ADROrePoolActor();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
        const FVector& SrcLocation) const override;

    // 풀링 시스템
    void ActivateFromPool(const FTransform& SpawnTransform, ADROreFieldActor* InOwningField,
        int32 InSpawnPointId);

    void DeactivateToPool();

    UFUNCTION(BlueprintPure, Category = "Ore Pool")
    bool IsPoolActive() const;

    UFUNCTION(BlueprintPure, Category = "Ore Pool")
    int32 GetSpawnPointId() const;

    UFUNCTION(BlueprintPure, Category = "Ore Pool")
    EDROreWorldState GetWorldState() const;

    // 완전 채굴된 광물을 물리 아이템으로 전환
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ore Pool")
    void MarkAsDropped();

    // Voxel 충돌 갱신 후 바로 아래 지면 확인
    void ScheduleGroundCheck(const FVector& DigLocation);

    ADROreFieldActor* GetOwningField() const;

protected:
    UFUNCTION()
    void OnRep_PoolState();

    UFUNCTION(BlueprintImplementableEvent, Category = "Ore Pool")
    void OnActivatedFromPool();

    UFUNCTION(BlueprintImplementableEvent, Category = "Ore Pool")
    void OnDeactivatedToPool();

private:
    UPROPERTY(ReplicatedUsing = OnRep_PoolState)
    bool bPoolActive = true;

    // 일반 월드 스폰은 Dropped 상태
    UPROPERTY(ReplicatedUsing = OnRep_PoolState)
    EDROreWorldState WorldState = EDROreWorldState::Dropped;

    // 채굴 위치에 박힌 광물의 클라이언트 표시 거리
    UPROPERTY(EditDefaultsOnly, Category = "Ore Pool", meta = (ClampMin = "0.0"))
    float EmbeddedNetCullDistance = 10000.f;

    // 완전 채굴되거나 던져진 광물의 클라이언트 표시 거리
    UPROPERTY(EditDefaultsOnly, Category = "Ore Pool", meta = (ClampMin = "0.0"))
    float DroppedNetCullDistance = 15000.f;

    // Embedded 광물 아래쪽 지면 탐색 거리
    UPROPERTY(EditDefaultsOnly, Category = "Ore Pool", meta = (ClampMin = "1.0"))
    float GroundCheckDistance = 5000.f;

    // Detached 전환과 지면 확인 간격
    UPROPERTY(EditDefaultsOnly, Category = "Ore Pool", meta = (ClampMin = "0.0"))
    float GroundCheckDelay = 0.2f;

    // Detached 상태 최대 유지 시간
    UPROPERTY(EditDefaultsOnly, Category = "Ore Pool", meta = (ClampMin = "0.0"))
    float DetachedTimeout = 5.f;

    // OreField 내부 스폰 위치 식별자
    UPROPERTY(ReplicatedUsing = OnRep_PoolState)
    int32 SpawnPointId = INDEX_NONE;

    // 해당 광물을 생성한 Field 참조
    UPROPERTY()
    TWeakObjectPtr<ADROreFieldActor> OwningField;
    FTimerHandle GroundCheckTimer;
    FTimerHandle DetachedTimeoutTimer;
    FVector DetachedStartLocation = FVector::ZeroVector;
    FVector LastDigLocation = FVector::ZeroVector;
    FVector LastGroundLocation = FVector::ZeroVector;
    bool bHasLastGroundLocation = false;

    void SetWorldState(EDROreWorldState NewState);
    void MarkAsDetached();
    void HandleDetachedTimeout();
    void CheckGroundBelow();
    void ApplyPoolState();

protected:
    virtual bool IsPickupAvailable() const override;
    virtual bool FinalizePickup() override;
    
};
