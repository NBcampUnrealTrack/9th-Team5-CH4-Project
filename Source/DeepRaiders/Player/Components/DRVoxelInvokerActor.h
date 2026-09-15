#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRVoxelInvokerActor.generated.h"

class UVoxelSimpleInvokerComponent;

// 서버에서 Voxel 충돌 범위를 유지하는 경량 Invoker
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRVoxelInvokerActor : public AActor
{
    GENERATED_BODY()

public:
    ADRVoxelInvokerActor();

    virtual void BeginPlay() override;

    void Configure(float CollisionRange);
    void FollowActor(AActor* TargetActor);
    void HoldLocation(const FVector& Location, float Duration);
    void SetLocation(const FVector& Location);
    void DisableInvoker();
    bool HasTarget() const;
    bool IsFollowing(const AActor* TargetActor) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Invoker")
    TObjectPtr<UVoxelSimpleInvokerComponent> InvokerComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel Invoker",
        meta = (ClampMin = "0.01"))
    float FollowUpdateInterval = 0.1f;

private:
    UPROPERTY()
    TWeakObjectPtr<AActor> FollowTarget;

    FTimerHandle DisableTimer;
    FTimerHandle FollowTimer;

    void EnableInvoker();
    void UpdateFollowLocation();
};
