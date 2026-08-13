#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRMeleeCombatComponent.generated.h"

class ADRPlayerCharacter;

UENUM(BlueprintType)
enum class EDRMeleeTraceMode : uint8
{
    ViewLine UMETA(DisplayName = "View Line"),
    WeaponSweep UMETA(DisplayName = "Weapon Sweep")
};

UCLASS(
    ClassGroup = (Player),
    meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRMeleeCombatComponent
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UDRMeleeCombatComponent();

    /** 소유 클라이언트가 근접 공격을 요청한다. */
    void RequestAttack();

    /** AnimNotifyState에서 Sweep Window를 연다. */
    void StartSweepWindow();

    /** AnimNotifyState가 활성화된 동안 매 프레임 호출한다. */
    void UpdateSweepWindow();

    /** AnimNotifyState에서 Sweep Window를 닫는다. */
    void EndSweepWindow();

    /** 사망 등으로 현재 공격을 강제 종료한다. */
    void CancelAttack();

    float GetAttackDuration() const
    {
        return MeleeAttackDuration;
    }

    bool IsAttacking() const
    {
        return bIsAttacking;
    }

private:
    ADRPlayerCharacter* GetOwnerCharacter() const;

    bool CanStartAttack() const;

    UFUNCTION(Server, Reliable)
    void ServerRequestAttack();

    void PerformHitCheck();

    void PerformLineTrace();

    void ProcessHit(
        const FHitResult& HitResult);

    void FinishAttack();

    void SweepSegment(
        const FVector& Start,
        const FVector& End);

    void SweepWeaponMotionFixedSamples(
        const FVector& PreviousBase,
        const FVector& PreviousTip,
        const FVector& CurrentBase,
        const FVector& CurrentTip);

private:
    bool bIsAttacking = false;
    bool bIsSweepActive = false;

    FTimerHandle MeleeHitTimerHandle;
    FTimerHandle MeleeFinishTimerHandle;

    TSet<TWeakObjectPtr<AActor>>
        AlreadyHitActors;

    FVector PreviousBaseLocation =
        FVector::ZeroVector;

    FVector PreviousTipLocation =
        FVector::ZeroVector;

protected:
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee")
    EDRMeleeTraceMode TraceMode =
        EDRMeleeTraceMode::ViewLine;

    /** ViewLine 방식에서 공격 시작 후 판정 시점 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee",
        meta = (ClampMin = "0.0"))
    float MeleeAttackHitTime = 0.25f;

    /** 다음 공격이 가능해지는 시간 */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee",
        meta = (ClampMin = "0.01"))
    float MeleeAttackDuration = 0.8f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee",
        meta = (ClampMin = "0.0"))
    float MeleeAttackDamage = 40.f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee",
        meta = (ClampMin = "0.0", Units = "cm"))
    float MeleeAttackRange = 200.f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee|Sweep")
    FName MeleeSweepBaseSocketName =
        TEXT("S_MeleeBase");

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee|Sweep")
    FName MeleeSweepTipSocketName =
        TEXT("S_MeleeTip");

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee|Sweep",
        meta = (ClampMin = "0.0", Units = "cm"))
    float MeleeSweepRadius = 35.f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee|Debug")
    bool bDrawDebug = false;
    
    /**
     * WeaponSweep에서 무기 이동 경로를 나눌
     * 최대 공간 간격.
     *
     * NotifyTick 간격과 무관하게 일정한 밀도로
     * 공격 궤적을 검사하기 위해 사용한다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee|Sweep",
        meta = (
            ClampMin = "1.0",
            Units = "cm"))
    float MeleeSweepSampleSpacing = 15.f;

    /**
     * 비정상적으로 큰 프레임 간격에서
     * 한 번에 너무 많은 Sweep이 발생하는 것을 방지한다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Melee|Sweep",
        meta = (
            ClampMin = "1",
            ClampMax = "64"))
    int32 MaxSweepSubstepsPerUpdate = 24;
};