#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRMeleeCombatComponent.generated.h"

class ADRPlayerCharacter;
class UAnimSequenceBase;
class UAnimMontage;
class UDRMeleeWeaponItemDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDRMeleeHitDetected,
    const FHitResult&);

UENUM(BlueprintType)
enum class EDRMeleeTraceMode : uint8
{
    ViewLine UMETA(DisplayName = "View Line"),
    WeaponSweep UMETA(DisplayName = "Weapon Sweep")
};

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRMeleeCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDRMeleeCombatComponent();

    /**
     * 서버 GameplayAbility가 공격 판정을 시작한다.
     */
    bool StartAttackFromAbility(UDRMeleeWeaponItemDefinition* WeaponDefinition);

    /**
     * GA 종료/취소 시 현재 판정을 종료한다.
     */
    void EndAttackFromAbility();

    bool CanStartAttackFromAbility(const UDRMeleeWeaponItemDefinition* WeaponDefinition) const;

    /** GA가 서버에서 Hit 결과를 받기 위한 Delegate */
    FDRMeleeHitDetected OnMeleeHitDetected;

    void SampleWeaponSweep(UAnimSequenceBase* Animation, float SampleTime);

    /** 사망 등 외부 시스템에 의한 강제 취소 */
    void CancelAttack();

    bool IsAttacking() const
    {
        return bIsAttacking;
    }

private:
    ADRPlayerCharacter* GetOwnerCharacter() const;
    
    void PerformHitCheck();
    void PerformLineTrace();
    void ProcessHit(const FHitResult& HitResult);
    void FinishAttack();
    void SweepSegment(const FVector& Start, const FVector& End);
    bool EvaluateWeaponSweepSample(const UAnimMontage* Montage, float SampleTime, FVector& OutBase, FVector& OutTip) const;
    
private:
    TWeakObjectPtr<UDRMeleeWeaponItemDefinition> ActiveWeaponDefinition;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Sweep")
    FName MeleeSweepBaseSocketName = TEXT("S_MeleeBase");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Sweep")
    FName MeleeSweepTipSocketName = TEXT("S_MeleeTip");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Debug")
    bool bDrawDebug = false;
    
};