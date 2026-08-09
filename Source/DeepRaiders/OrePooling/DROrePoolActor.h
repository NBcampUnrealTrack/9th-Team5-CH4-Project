#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DROrePoolActor.generated.h"

class FLifetimeProperty;
class ADROreFieldActor;

// 서버에서 풀링되며 활성 상태가 복제된다.
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADROrePoolActor : public ADRWorldItemActor, public IDRInteractableInterface
{
    GENERATED_BODY()

public:
    ADROrePoolActor();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void ActivateFromPool(const FTransform& SpawnTransform, int32 InSpawnPointId);

    void DeactivateToPool();

    UFUNCTION(BlueprintPure, Category = "Ore Pool")
    bool IsPoolActive() const;

    UFUNCTION(BlueprintPure, Category = "Ore Pool")
    int32 GetSpawnPointId() const;
    
    void AssignSourceField(ADROreFieldActor* InSourceField);

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

    // OreField 내 생성 위치 식별자
    UPROPERTY(ReplicatedUsing = OnRep_PoolState)
    int32 SpawnPointId = INDEX_NONE;

    void ApplyPoolState();
    
    // OreFieldActor 와의 연결을 끊기 위해서 캐싱
    UPROPERTY(Transient)
    TWeakObjectPtr<ADROreFieldActor> SourceField;
    
#pragma region Interactable
public:
    bool CanInteract_Implementation(APawn* Interactor) const override;
    bool Interact_Implementation(APawn* Interactor) override;
   
private:
    uint8 bInteractionInProgress:1 = false;
#pragma endregion
};
