#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Item/DRItemActionTypes.h"
#include "DRHeldItemComponent.generated.h"

class ADRPlayerCharacter;
class UDRItemDefinition;
class USoundBase;

UCLASS(
    ClassGroup = (Player),
    meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRHeldItemComponent
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UDRHeldItemComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps)
        const override;

    /**
     * QuickSlot이 서버에서 결정한
     * 현재 손 아이템을 적용한다.
     */
    void SetHeldItemDefinition(
        UDRItemDefinition* NewItemDefinition);

    UDRItemDefinition*
    GetHeldItemDefinition() const
    {
        return HeldItemDefinition;
    }

    bool HasAction(
        EDRItemActionType ActionType) const;

    void RequestPrimaryAction(
        EDRItemActionTriggerEvent TriggerEvent);

    void RequestSecondaryAction(
        EDRItemActionTriggerEvent TriggerEvent);

private:
    ADRPlayerCharacter*
    GetOwnerCharacter() const;

    void ExecuteAction(
        EDRItemActionType ActionType);

    bool CanStartLocalAction() const;

    float GetActionCooldown(
        EDRItemActionType ActionType) const;

    void RefreshHeldItemState();

    void RefreshVisual();

    void RefreshMiningSettings();

    void PlayEquipSound();

    UFUNCTION()
    void OnRep_HeldItemDefinition();

private:
    UPROPERTY(
        ReplicatedUsing = OnRep_HeldItemDefinition)
    TObjectPtr<UDRItemDefinition>
        HeldItemDefinition;

    float NextLocalActionTime = 0.f;

protected:
    /**
     * 채굴 Action의 로컬 입력 쿨다운.
     *
     * Melee는 MeleeCombatComponent의
     * AttackDuration을 사용한다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Held Item|Action",
        meta = (ClampMin = "0.01"))
    float DigActionCooldown = 0.6f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Held Item|Sound")
    TObjectPtr<USoundBase>
        EquipSound;
};