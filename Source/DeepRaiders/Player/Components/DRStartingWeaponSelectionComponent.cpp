#include "DRStartingWeaponSelectionComponent.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Item/DRStartingWeaponTable.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "Engine/DataTable.h"

UDRStartingWeaponSelectionComponent::UDRStartingWeaponSelectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRStartingWeaponSelectionComponent::Initialize(
	UDataTable* InWeaponTable,
	UDRItemDefinition* InStartingWeaponDefinition,
	UDRInventoryComponent* InInventoryComponent,
	UDRQuickSlotComponent* InQuickSlotComponent)
{
	WeaponTable = InWeaponTable;
	StartingWeaponDefinition = InStartingWeaponDefinition;
	InventoryComponent = InInventoryComponent;
	QuickSlotComponent = InQuickSlotComponent;
}

void UDRStartingWeaponSelectionComponent::RequestSelection(FName RowName)
{
	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());

	// UI 요청은 로컬 소유자만 서버 RPC로 전달할 수 있다.
	if (IsValid(PlayerController)
		&& PlayerController->IsLocalController()
		&& !RowName.IsNone())
	{
		ServerSelectWeapon(RowName);
	}
}

void UDRStartingWeaponSelectionComponent::ExpireSelection()
{
	if (IsSelectionAvailable())
	{
		SetSelectionState(EDRStartingWeaponSelectionState::Expired);
	}
}

void UDRStartingWeaponSelectionComponent::ServerSelectWeapon_Implementation(FName RowName)
{
	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());

	// 클라이언트 요청을 신뢰하지 않고 선택 기회와 상점 범위를 서버에서 다시 확인한다.
	if (!IsSelectionAvailable()
		|| !IsValid(PlayerController)
		|| !PlayerController->IsShopInteractionAvailable()
		|| RowName.IsNone()
		|| !IsValid(WeaponTable)
		|| !IsValid(StartingWeaponDefinition)
		|| !IsValid(InventoryComponent)
		|| !IsValid(QuickSlotComponent))
	{
		return;
	}

	const FDRStartingWeaponTableRow* Row =
		WeaponTable->FindRow<FDRStartingWeaponTableRow>(RowName, TEXT("StartingWeaponSelection"));
	UDRProjectileWeaponItemDefinition* SelectedWeapon = Row
		? Row->WeaponDefinition.LoadSynchronous()
		: nullptr;

	if (!IsValid(SelectedWeapon) || !TryApplySelection(SelectedWeapon))
	{
		return;
	}

	SetSelectionState(EDRStartingWeaponSelectionState::Selected);
	ClientCompleteSelection();
}

void UDRStartingWeaponSelectionComponent::ClientCompleteSelection_Implementation()
{
	SetSelectionState(EDRStartingWeaponSelectionState::Selected);
}

void UDRStartingWeaponSelectionComponent::SetSelectionState(
	EDRStartingWeaponSelectionState NewState)
{
	if (SelectionState == NewState)
	{
		return;
	}

	SelectionState = NewState;
	// UI를 직접 참조하지 않고 이벤트로 현재 열린 상점 화면만 갱신한다.
	OnSelectionAvailabilityChanged.Broadcast(IsSelectionAvailable());
}

bool UDRStartingWeaponSelectionComponent::TryApplySelection(
	UDRItemDefinition* SelectedWeapon)
{
	int32 WeaponSlotIndex = INDEX_NONE;
	const FDRItemInstance* CurrentWeapon = nullptr;

	for (int32 SlotIndex = 0; SlotIndex < InventoryComponent->GetMaxSlots(); ++SlotIndex)
	{
		const FDRItemInstance* ItemInstance = InventoryComponent->GetItemAtSlot(SlotIndex);

		// 최초 지급에 사용한 Definition과 정확히 같은 아이템만 교체 대상으로 인정한다.
		if (ItemInstance
			&& ItemInstance->Definition.Get() == StartingWeaponDefinition)
		{
			WeaponSlotIndex = SlotIndex;
			CurrentWeapon = ItemInstance;
			break;
		}
	}

	if (WeaponSlotIndex == INDEX_NONE || !CurrentWeapon)
	{
		for (int32 SlotIndex = 0; SlotIndex < InventoryComponent->GetMaxSlots(); ++SlotIndex)
		{
			if (!InventoryComponent->GetItemAtSlot(SlotIndex))
			{
				const bool IsAdded = InventoryComponent->TryAddItemToSlot(
					SlotIndex,
					SelectedWeapon,
					1);

				if (IsAdded)
				{
					QuickSlotComponent->RequestSelectSlot(SlotIndex);
				}

				return IsAdded;
			}
		}

		return false;
	}

	// 기본 총을 그대로 선택한 경우에는 불필요한 인벤토리 변경을 생략한다.
	const bool IsApplied = StartingWeaponDefinition == SelectedWeapon
		|| InventoryComponent->TryReplaceItemDefinition(
			CurrentWeapon->InstanceId,
			StartingWeaponDefinition,
			SelectedWeapon);

	if (!IsApplied)
	{
		return false;
	}

	// UI상의 슬롯 위치가 바뀌지 않도록 교체가 발생한 기존 슬롯을 그대로 선택한다.
	QuickSlotComponent->RequestSelectSlot(WeaponSlotIndex);
	return true;
}
