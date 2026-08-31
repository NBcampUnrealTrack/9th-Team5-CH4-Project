#include "DRShopBuyPanelWidget.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "DeepRaiders/UI/Inventory/DRInventoryWidget.h"
#include "DeepRaiders/UI/ViewModel/DRShopViewModel.h"
#include "DRShopItemWidget.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRShopBuyPanelWidget::InitializeInventory(UDRInventoryComponent* InventoryComponent)
{
	if (IsValid(PlayerInventory))
	{
		PlayerInventory->InitializeInventory(InventoryComponent);
	}
}

void UDRShopBuyPanelWidget::SetOffers(
	EDRShopOfferType OfferType,
	const TArray<FDRShopOfferView>& NewOffers)
{
	if (IsValid(ShopViewModel))
	{
		ShopViewModel->SetOffers(OfferType, NewOffers);
		SetOfferEntries(ShopViewModel->GetOfferEntries());
	}
}

void UDRShopBuyPanelWidget::SetOfferEntries(
	const TArray<UDRShopOfferEntryViewModel*>& NewOfferEntries)
{
	if (!IsValid(ItemScrollBox) || !ItemWidgetClass)
	{
		return;
	}

	ItemScrollBox->ClearChildren();

	for (UDRShopOfferEntryViewModel* EntryViewModel : NewOfferEntries)
	{
		if (!IsValid(EntryViewModel))
		{
			continue;
		}

		UDRShopItemWidget* ItemWidget = CreateWidget<UDRShopItemWidget>(
			GetOwningPlayer(),
			ItemWidgetClass);

		if (!IsValid(ItemWidget))
		{
			continue;
		}

		ItemWidget->InitializeViewModel(EntryViewModel);
		ItemWidget->OnOfferRequested.AddDynamic(this, &ThisClass::HandleOfferRequested);
		ItemScrollBox->AddChild(ItemWidget);
	}
}

void UDRShopBuyPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ShopViewModel = NewObject<UDRShopViewModel>(this);
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);

	if (!IsValid(View) || !View->SetViewModel(ShopViewModelName, ShopViewModel))
	{
		UE_LOG(LogTemp, Warning, TEXT("Shop ViewModel '%s' is not registered on %s"),
			*ShopViewModelName.ToString(), *GetName());
	}

	if (IsValid(EquipmentButton))
	{
		EquipmentButton->OnClicked.AddDynamic(this, &ThisClass::HandleEquipmentButtonClicked);
	}

	if (IsValid(ConsumableButton))
	{
		ConsumableButton->OnClicked.AddDynamic(this, &ThisClass::HandleConsumableButtonClicked);
	}

	if (IsValid(UpgradeButton))
	{
		UpgradeButton->OnClicked.AddDynamic(this, &ThisClass::HandleUpgradeButtonClicked);
	}

	if (IsValid(PerkButton))
	{
		PerkButton->OnClicked.AddDynamic(this, &ThisClass::HandlePerkButtonClicked);
	}

	SelectSection(SelectedSection);
}

void UDRShopBuyPanelWidget::NativeDestruct()
{
	if (IsValid(ShopViewModel))
	{
		ShopViewModel->Deinitialize();
	}

	if (IsValid(EquipmentButton))
	{
		EquipmentButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleEquipmentButtonClicked);
	}

	if (IsValid(ConsumableButton))
	{
		ConsumableButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConsumableButtonClicked);
	}

	if (IsValid(UpgradeButton))
	{
		UpgradeButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleUpgradeButtonClicked);
	}

	if (IsValid(PerkButton))
	{
		PerkButton->OnClicked.RemoveDynamic(this, &ThisClass::HandlePerkButtonClicked);
	}

	Super::NativeDestruct();
}

void UDRShopBuyPanelWidget::SelectSection(EDRShopOfferSection Section)
{
	if (!IsValid(EquipmentButton)
		|| !IsValid(ConsumableButton)
		|| !IsValid(UpgradeButton)
		|| !IsValid(PerkButton))
	{
		return;
	}

	SelectedSection = Section;

	if (IsValid(ShopViewModel))
	{
		ShopViewModel->SelectSection(Section);
		SetOfferEntries(ShopViewModel->GetOfferEntries());
	}

	EquipmentButton->SetIsEnabled(Section != EDRShopOfferSection::Equipment);
	ConsumableButton->SetIsEnabled(Section != EDRShopOfferSection::Consumable);
	UpgradeButton->SetIsEnabled(Section != EDRShopOfferSection::Upgrade);
	PerkButton->SetIsEnabled(Section != EDRShopOfferSection::Perk);
}

void UDRShopBuyPanelWidget::HandleEquipmentButtonClicked()
{
	SelectSection(EDRShopOfferSection::Equipment);
}

void UDRShopBuyPanelWidget::HandleConsumableButtonClicked()
{
	SelectSection(EDRShopOfferSection::Consumable);
}

void UDRShopBuyPanelWidget::HandleUpgradeButtonClicked()
{
	SelectSection(EDRShopOfferSection::Upgrade);
}

void UDRShopBuyPanelWidget::HandlePerkButtonClicked()
{
	SelectSection(EDRShopOfferSection::Perk);
}

void UDRShopBuyPanelWidget::HandleOfferRequested(FDRShopOfferRequest Request)
{
	OnOfferRequested.Broadcast(Request);
}
