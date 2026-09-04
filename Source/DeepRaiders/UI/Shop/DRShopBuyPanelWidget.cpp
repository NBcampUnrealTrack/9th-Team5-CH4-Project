#include "DRShopBuyPanelWidget.h"

#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "DeepRaiders/UI/Inventory/DRInventoryWidget.h"
#include "DeepRaiders/UI/ViewModel/DRShopViewModel.h"
#include "DRShopItemWidget.h"
#include "DRShopWeaponUpgradeWidget.h"
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
	if (!IsValid(ItemScrollBox))
	{
		return;
	}

	ItemScrollBox->ClearChildren();
	if (SelectedSection == EDRShopOfferSection::WeaponUpgrade)
	{
		SetWeaponUpgradeEntries(NewOfferEntries);
		return;
	}
	if (!ItemWidgetClass)
	{
		return;
	}

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

void UDRShopBuyPanelWidget::SetWeaponUpgradeEntries(const TArray<UDRShopOfferEntryViewModel*>& NewOfferEntries)
{
	if (!WeaponUpgradeWidgetClass)
	{
		return;
	}
	TArray<FGuid> WeaponIds;
	TMap<FGuid, TArray<FDRShopOfferView>> WeaponOffers;
	for (const UDRShopOfferEntryViewModel* Entry : NewOfferEntries)
	{
		if (!IsValid(Entry))
		{
			continue;
		}
		const FDRShopOfferView& Offer = Entry->GetOffer();
		if (!WeaponOffers.Contains(Offer.Request.InstanceId))
		{
			WeaponIds.Add(Offer.Request.InstanceId);
		}
		WeaponOffers.FindOrAdd(Offer.Request.InstanceId).Add(Offer);
	}

	for (const FGuid& InstanceId : WeaponIds)
	{
		UDRShopWeaponUpgradeWidget* WeaponWidget = CreateWidget<UDRShopWeaponUpgradeWidget>(GetOwningPlayer(), WeaponUpgradeWidgetClass);
		if (!IsValid(WeaponWidget))
		{
			continue;
		}
		WeaponWidget->InitializeOffers(WeaponOffers[InstanceId]);
		WeaponWidget->OnOfferRequested.AddDynamic(this, &ThisClass::HandleOfferRequested);
		ItemScrollBox->AddChild(WeaponWidget);
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

	if (IsValid(PerkButton))
	{
		PerkButton->OnClicked.AddDynamic(this, &ThisClass::HandlePerkButtonClicked);
	}

	if (!IsValid(CharacterUpgradeButton) && IsValid(PerkButton) && PerkButton->GetParent())
	{
		CharacterUpgradeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CharacterUpgradeButton"));
		CharacterUpgradeButton->SetStyle(PerkButton->GetStyle());
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(NSLOCTEXT("Shop", "CharacterUpgradeTab", "스탯 강화"));
		if (const UTextBlock* PerkLabel = Cast<UTextBlock>(PerkButton->GetContent()))
		{
			Label->SetFont(PerkLabel->GetFont());
			Label->SetColorAndOpacity(PerkLabel->GetColorAndOpacity());
		}
		CharacterUpgradeButton->AddChild(Label);
		PerkButton->GetParent()->AddChild(CharacterUpgradeButton);
	}
	if (IsValid(CharacterUpgradeButton))
	{
		CharacterUpgradeButton->OnClicked.AddDynamic(this, &ThisClass::HandleCharacterUpgradeButtonClicked);
	}
	if (!IsValid(WeaponUpgradeButton) && IsValid(PerkButton) && PerkButton->GetParent())
	{
		WeaponUpgradeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("WeaponUpgradeButton"));
		WeaponUpgradeButton->SetStyle(PerkButton->GetStyle());
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(NSLOCTEXT("Shop", "WeaponUpgradeTab", "무기 강화"));
		if (const UTextBlock* PerkLabel = Cast<UTextBlock>(PerkButton->GetContent()))
		{
			Label->SetFont(PerkLabel->GetFont());
			Label->SetColorAndOpacity(PerkLabel->GetColorAndOpacity());
		}
		WeaponUpgradeButton->AddChild(Label);
		PerkButton->GetParent()->AddChild(WeaponUpgradeButton);
	}
	if (IsValid(WeaponUpgradeButton))
	{
		WeaponUpgradeButton->OnClicked.AddDynamic(this, &ThisClass::HandleWeaponUpgradeButtonClicked);
	}
	SelectSection(SelectedSection);
}

void UDRShopBuyPanelWidget::NativeDestruct()
{
	if (IsValid(WeaponUpgradeButton))
	{
		WeaponUpgradeButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleWeaponUpgradeButtonClicked);
	}
	if (IsValid(CharacterUpgradeButton))
	{
		CharacterUpgradeButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCharacterUpgradeButtonClicked);
	}
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
	PerkButton->SetIsEnabled(Section != EDRShopOfferSection::Perk);
	if (IsValid(WeaponUpgradeButton))
	{
		WeaponUpgradeButton->SetIsEnabled(Section != EDRShopOfferSection::WeaponUpgrade);
	}
	if (IsValid(CharacterUpgradeButton))
	{
		CharacterUpgradeButton->SetIsEnabled(Section != EDRShopOfferSection::CharacterUpgrade);
	}
}

void UDRShopBuyPanelWidget::HandleCharacterUpgradeButtonClicked()
{
	SelectSection(EDRShopOfferSection::CharacterUpgrade);
}

void UDRShopBuyPanelWidget::HandleWeaponUpgradeButtonClicked()
{
	SelectSection(EDRShopOfferSection::WeaponUpgrade);
}

void UDRShopBuyPanelWidget::HandleEquipmentButtonClicked()
{
	SelectSection(EDRShopOfferSection::Equipment);
}

void UDRShopBuyPanelWidget::HandleConsumableButtonClicked()
{
	SelectSection(EDRShopOfferSection::Consumable);
}


void UDRShopBuyPanelWidget::HandlePerkButtonClicked()
{
	SelectSection(EDRShopOfferSection::Perk);
}

void UDRShopBuyPanelWidget::HandleOfferRequested(FDRShopOfferRequest Request)
{
	OnOfferRequested.Broadcast(Request);
}
