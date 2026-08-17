#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRMeleeCombatComponent.generated.h"

class ADRPlayerCharacter;
class UAnimSequenceBase;
class UAnimMontage;

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

    /**
     * 공격 Montage의 고정 Sweep Notify에서 호출된다.
     *
     * 현재 Runtime Pose를 읽지 않고
     * SampleTime에 해당하는 Animation Pose를 직접 평가한다.
     */
    void SampleWeaponSweep(UAnimSequenceBase* Animation, float SampleTime);

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

    void ProcessHit(const FHitResult& HitResult);

    void FinishAttack();

    void SweepSegment(const FVector& Start, const FVector& End);

    bool EvaluateWeaponSweepSample(const UAnimMontage* Montage, float SampleTime, FVector& OutBase, FVector& OutTip) const;
    
private:
    bool bIsAttacking = false;
    bool bHasPreviousSweepSample = false;

    FTimerHandle MeleeHitTimerHandle;
    FTimerHandle MeleeFinishTimerHandle;

    TSet<TWeakObjectPtr<AActor>> AlreadyHitActors;

    FVector PreviousBaseLocation = FVector::ZeroVector;
    FVector PreviousTipLocation = FVector::ZeroVector;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
    EDRMeleeTraceMode TraceMode = EDRMeleeTraceMode::ViewLine;

    /** ViewLine 방식에서 공격 시작 후 판정 시점 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.0"))
    float MeleeAttackHitTime = 0.25f;

    /** 다음 공격이 가능해지는 시간 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.01"))
    float MeleeAttackDuration = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.0"))
    float MeleeAttackDamage = 40.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.0", Units = "cm"))
    float MeleeAttackRange = 200.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Sweep")
    FName MeleeSweepBaseSocketName = TEXT("S_MeleeBase");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Sweep")
    FName MeleeSweepTipSocketName = TEXT("S_MeleeTip");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Sweep", meta = (ClampMin = "0.0", Units = "cm"))
    float MeleeSweepRadius = 35.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Debug")
    bool bDrawDebug = false;
    
};