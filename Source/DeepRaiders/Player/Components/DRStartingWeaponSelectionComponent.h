#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRStartingWeaponSelectionComponent.generated.h"

class UDataTable;
class UDRInventoryComponent;
class UDRItemDefinition;
class UDRQuickSlotComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FDRStartingWeaponSelectionAvailabilityChanged,
	bool);

/** 한 번만 가능한 최초 무기 선택의 진행 상태다. */
enum class EDRStartingWeaponSelectionState : uint8
{
	Available,
	Selected,
	Expired
};

/** 최초 무기 선택 상태와 기본 무기 교체를 관리한다. */
UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRStartingWeaponSelectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRStartingWeaponSelectionComponent();

	/** 기본 무기와 장비 컴포넌트를 연결한다. */
	void Initialize(
		UDRItemDefinition* InStartingWeaponDefinition,
		UDRInventoryComponent* InInventoryComponent,
		UDRQuickSlotComponent* InQuickSlotComponent);


	/** 로컬 플레이어가 선택한 DT Row를 서버에 요청한다. */
	void RequestSelection(FName RowName);

	/** 상점 영역을 벗어난 최초 1회 선택 기회를 만료시킨다. */
	void ExpireSelection();

	bool IsSelectionAvailable() const
	{
		return SelectionState == EDRStartingWeaponSelectionState::Available;
	}

	/** UI가 선택 목록을 구성할 때 사용하는 DT다. */
	UDataTable* GetWeaponTable() const { return WeaponTable; }

	/** 선택 완료 또는 만료로 사용 가능 상태가 변경될 때 알린다. */
	FDRStartingWeaponSelectionAvailabilityChanged OnSelectionAvailabilityChanged;

private:
	/** 선택 권한과 상점 범위를 서버에서 검증한 뒤 무기를 교체한다. */
	UFUNCTION(Server, Reliable)
	void ServerSelectWeapon(FName RowName);

	/** 서버에서 확정된 선택 상태를 소유 클라이언트에 반영한다. */
	UFUNCTION(Client, Reliable)
	void ClientCompleteSelection();

	void SetSelectionState(EDRStartingWeaponSelectionState NewState);

	/** 기본 총은 같은 슬롯에서 교체하고, 없으면 가장 앞의 빈 슬롯에 지급한다. */
	bool TryApplySelection(UDRItemDefinition* SelectedWeapon);

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Starting Weapon",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/DeepRaiders.DRStartingWeaponTableRow"))
	TObjectPtr<UDataTable> WeaponTable;

	UPROPERTY(Transient)
	TObjectPtr<UDRItemDefinition> StartingWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;

	EDRStartingWeaponSelectionState SelectionState =
		EDRStartingWeaponSelectionState::Available;
};
