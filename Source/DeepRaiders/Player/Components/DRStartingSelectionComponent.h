#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "DRStartingSelectionComponent.generated.h"

class UDataTable;
class UDRInventoryComponent;
class UDRItemDefinition;
class UDRQuickSlotComponent;
class UDRSkillDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FDRStartingSelectionAvailabilityChanged,
	bool);

/** 최초 무기·스킬 선택 데이터와 적용을 관리한다. */
UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRStartingSelectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRStartingSelectionComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Initialize(
		UDRItemDefinition* InStartingWeaponDefinition,
		UDRInventoryComponent* InInventoryComponent,
		UDRQuickSlotComponent* InQuickSlotComponent);

	void RequestWeaponSelection(FName RowName);
	void RequestSkillSelection(FName RowName);

	bool IsWeaponSelectionAvailable() const
	{
		return IsWeaponSelectionEnabled && !IsWeaponSelected;
	}

	bool IsSkillSelectionAvailable() const
	{
		return !IsSkillOneSelected || !IsSkillTwoSelected;
	}

	EDRSkillSlot GetPendingSkillSlot() const;

	bool IsSelectionComplete() const
	{
		return !IsWeaponSelectionAvailable() && !IsSkillSelectionAvailable();
	}

	bool IsSelectionAvailable() const
	{
		return !IsSelectionComplete();
	}

	UDataTable* GetWeaponTable() const { return WeaponTable; }
	UDataTable* GetSkillTable() const { return SkillTable; }

	FDRStartingSelectionAvailabilityChanged OnSelectionAvailabilityChanged;

private:
	UFUNCTION(Server, Reliable)
	void ServerSelectWeapon(FName RowName);

	UFUNCTION(Server, Reliable)
	void ServerSelectSkill(FName RowName);

	UFUNCTION()
	void OnRep_SelectionState();

	void NotifySelectionStateChanged();
	bool TryApplySelection(UDRItemDefinition* SelectedWeapon);

	UPROPERTY(EditDefaultsOnly, Category = "Starting Selection|Weapon")
	bool IsWeaponSelectionEnabled = false;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Starting Selection|Weapon",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/DeepRaiders.DRStartingWeaponTableRow"))
	TObjectPtr<UDataTable> WeaponTable;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Starting Selection|Skill",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/DeepRaiders.DRStartingSkillTableRow"))
	TObjectPtr<UDataTable> SkillTable;

	UPROPERTY(Transient)
	TObjectPtr<UDRItemDefinition> CurrentWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;

	UPROPERTY(ReplicatedUsing = OnRep_SelectionState)
	bool IsWeaponSelected = false;

	UPROPERTY(ReplicatedUsing = OnRep_SelectionState)
	bool IsSkillOneSelected = false;

	UPROPERTY(ReplicatedUsing = OnRep_SelectionState)
	bool IsSkillTwoSelected = false;
};
