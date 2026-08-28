#include "DRStartingSelectionComponent.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Item/DRStartingWeaponTable.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Skill/Components/DRSkillComponent.h"
#include "DeepRaiders/Skill/DRSkillDefinition.h"
#include "DeepRaiders/Skill/DRStartingSkillTable.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"

UDRStartingSelectionComponent::UDRStartingSelectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDRStartingSelectionComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UDRStartingSelectionComponent,
		IsWeaponSelected,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		UDRStartingSelectionComponent,
		IsSkillOneSelected,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		UDRStartingSelectionComponent,
		IsSkillTwoSelected,
		COND_OwnerOnly);
}

EDRSkillSlot UDRStartingSelectionComponent::GetPendingSkillSlot() const
{
	if (!IsSkillOneSelected)
	{
		return EDRSkillSlot::One;
	}

	return !IsSkillTwoSelected
		? EDRSkillSlot::Two
		: EDRSkillSlot::Count;
}

void UDRStartingSelectionComponent::Initialize(
	UDRItemDefinition* InStartingWeaponDefinition,
	UDRInventoryComponent* InInventoryComponent,
	UDRQuickSlotComponent* InQuickSlotComponent)
{
	CurrentWeaponDefinition = InStartingWeaponDefinition;
	InventoryComponent = InInventoryComponent;
	QuickSlotComponent = InQuickSlotComponent;
}

void UDRStartingSelectionComponent::RequestWeaponSelection(FName RowName)
{
	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());

	if (IsValid(PlayerController)
		&& PlayerController->IsLocalController()
		&& !RowName.IsNone())
	{
		ServerSelectWeapon(RowName);
	}
}

void UDRStartingSelectionComponent::RequestSkillSelection(FName RowName)
{
	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());

	if (IsValid(PlayerController)
		&& PlayerController->IsLocalController()
		&& !RowName.IsNone())
	{
		ServerSelectSkill(RowName);
	}
}

void UDRStartingSelectionComponent::ServerSelectWeapon_Implementation(FName RowName)
{
	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());

	if (!IsWeaponSelectionAvailable()
		|| !IsValid(PlayerController)
		|| RowName.IsNone()
		|| !IsValid(WeaponTable)
		|| !IsValid(CurrentWeaponDefinition)
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

	if (IsValid(SelectedWeapon))
	{
		IsWeaponSelected = TryApplySelection(SelectedWeapon);

		if (IsWeaponSelected)
		{
			NotifySelectionStateChanged();
		}
	}
}

void UDRStartingSelectionComponent::ServerSelectSkill_Implementation(FName RowName)
{
	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	ADRPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADRPlayerState>()
		: nullptr;
	UDRSkillComponent* SkillComponent = IsValid(PlayerState)
		? PlayerState->GetSkillComponent()
		: nullptr;
	const FDRStartingSkillTableRow* Row = IsValid(SkillTable)
		? SkillTable->FindRow<FDRStartingSkillTableRow>(RowName, TEXT("StartingSkillSelection"))
		: nullptr;
	UDRSkillDefinition* SkillDefinition = Row
		? Row->SkillDefinition.LoadSynchronous()
		: nullptr;

	if (!IsSkillSelectionAvailable()
		|| !IsValid(SkillDefinition)
		|| !IsValid(SkillComponent)
		|| SkillDefinition->SkillSlot != GetPendingSkillSlot())
	{
		return;
	}

	const bool IsSkillEquipped = SkillComponent->EquipSkill(SkillDefinition);

	if (IsSkillEquipped)
	{
		if (SkillDefinition->SkillSlot == EDRSkillSlot::One)
		{
			IsSkillOneSelected = true;
		}
		else
		{
			IsSkillTwoSelected = true;
		}

		NotifySelectionStateChanged();
	}
}

void UDRStartingSelectionComponent::OnRep_SelectionState()
{
	NotifySelectionStateChanged();
}

void UDRStartingSelectionComponent::NotifySelectionStateChanged()
{
	OnSelectionAvailabilityChanged.Broadcast(IsSelectionAvailable());
}

bool UDRStartingSelectionComponent::TryApplySelection(
	UDRItemDefinition* SelectedWeapon)
{
	int32 WeaponSlotIndex = INDEX_NONE;
	const FDRItemInstance* CurrentWeapon = nullptr;

	for (int32 SlotIndex = 0; SlotIndex < InventoryComponent->GetMaxSlots(); ++SlotIndex)
	{
		const FDRItemInstance* ItemInstance = InventoryComponent->GetItemAtSlot(SlotIndex);

		if (ItemInstance
			&& ItemInstance->Definition.Get() == CurrentWeaponDefinition)
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
					CurrentWeaponDefinition = SelectedWeapon;
					QuickSlotComponent->RequestSelectSlot(SlotIndex);
				}

				return IsAdded;
			}
		}

		return false;
	}

	const bool IsApplied = CurrentWeaponDefinition == SelectedWeapon
		|| InventoryComponent->TryReplaceItemDefinition(
			CurrentWeapon->InstanceId,
			CurrentWeaponDefinition,
			SelectedWeapon);

	if (!IsApplied)
	{
		return false;
	}

	CurrentWeaponDefinition = SelectedWeapon;
	QuickSlotComponent->RequestSelectSlot(WeaponSlotIndex);
	return true;
}
