#include "DRShopSellPanelWidget.h"

#include "DeepRaiders/UI/Inventory/DRInventoryWidget.h"
#include "DeepRaiders/UI/ViewModel/DRShopSellViewModel.h"
#include "DRShopSellItemInfoWidget.h"

void UDRShopSellPanelWidget::InitializeInventory(
	UDRInventoryComponent* NewInventoryComponent,
	UDRPerkComponent* NewPerkComponent)
{
	if (!IsValid(SellViewModel))
	{
		SellViewModel = NewObject<UDRShopSellViewModel>(this);
	}

	SellViewModel->Initialize(NewInventoryComponent, NewPerkComponent);

	if (IsValid(InventoryPanel))
	{
		InventoryPanel->InitializeInventory(NewInventoryComponent);
	}

	if (IsValid(ItemInfoPanel))
	{
		ItemInfoPanel->InitializeViewModel(SellViewModel);
	}
}

void UDRShopSellPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(InventoryPanel))
	{
		InventoryPanel->OnEntryClickedDelegate.AddUniqueDynamic(this, &ThisClass::HandleEntryClicked);
		InventoryPanel->OnPerkClickedDelegate.AddUniqueDynamic(this, &ThisClass::HandlePerkClicked);
	}

	if (IsValid(ItemInfoPanel))
	{
		ItemInfoPanel->OnSellRequested.AddUniqueDynamic(this, &ThisClass::HandleSellRequested);
	}
}

void UDRShopSellPanelWidget::NativeDestruct()
{
	if (IsValid(InventoryPanel))
	{
		InventoryPanel->OnEntryClickedDelegate.RemoveDynamic(this, &ThisClass::HandleEntryClicked);
		InventoryPanel->OnPerkClickedDelegate.RemoveDynamic(this, &ThisClass::HandlePerkClicked);
	}

	if (IsValid(ItemInfoPanel))
	{
		ItemInfoPanel->OnSellRequested.RemoveDynamic(this, &ThisClass::HandleSellRequested);
	}

	if (IsValid(SellViewModel))
	{
		SellViewModel->Deinitialize();
	}

	SellViewModel = nullptr;
	Super::NativeDestruct();
}

void UDRShopSellPanelWidget::HandleEntryClicked(FGuid InstanceId)
{
	if (IsValid(SellViewModel))
	{
		SellViewModel->SelectItem(InstanceId);
	}
}

void UDRShopSellPanelWidget::HandlePerkClicked(FGuid PerkInstanceId)
{
	if (IsValid(SellViewModel))
	{
		SellViewModel->SelectPerk(PerkInstanceId);
	}
}

void UDRShopSellPanelWidget::HandleSellRequested(
	EDRShopSellTargetType TargetType,
	FGuid InstanceId)
{
	OnSellRequested.Broadcast(TargetType, InstanceId);
}
