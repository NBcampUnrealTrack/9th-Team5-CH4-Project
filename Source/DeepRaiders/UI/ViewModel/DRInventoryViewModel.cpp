#include "DRInventoryViewModel.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "GameFramework/GameStateBase.h"

void UDRInventorySlotEntryViewModel::Initialize(
	UDRInventoryComponent* InInventoryComponent,
	int32 InSlotIndex)
{
	InventoryComponent = InInventoryComponent;
	UE_MVVM_SET_PROPERTY_VALUE(SlotIndex, InSlotIndex);
	UE_MVVM_SET_PROPERTY_VALUE(SlotNumberText, FText::AsNumber(InSlotIndex + 1));
	Refresh();
}

void UDRInventorySlotEntryViewModel::Initialize(
	const FDRPublicInventorySlot& InSlot,
	int32 InSlotIndex)
{
	InventoryComponent.Reset();
	UE_MVVM_SET_PROPERTY_VALUE(SlotIndex, InSlotIndex);
	UE_MVVM_SET_PROPERTY_VALUE(SlotNumberText, FText::AsNumber(InSlotIndex + 1));
	UE_MVVM_SET_PROPERTY_VALUE(InstanceId, FGuid());
	UE_MVVM_SET_PROPERTY_VALUE(ItemDefinition, InSlot.ItemDefinition);
	UE_MVVM_SET_PROPERTY_VALUE(
		ItemIcon,
		IsValid(InSlot.ItemDefinition) ? InSlot.ItemDefinition->Icon.Get() : nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(Quantity, InSlot.Quantity);
	UE_MVVM_SET_PROPERTY_VALUE(
		QuantityText,
		InSlot.Quantity > 1 ? FText::AsNumber(InSlot.Quantity) : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bHasItem, IsValid(InSlot.ItemDefinition) && InSlot.Quantity > 0);
	UE_MVVM_SET_PROPERTY_VALUE(bIsLocked, InSlot.bIsLocked);
}

void UDRInventorySlotEntryViewModel::Refresh()
{
	const UDRInventoryComponent* Inventory = InventoryComponent.Get();
	const FDRItemInstance* Item = IsValid(Inventory)
		? Inventory->GetItemAtSlot(SlotIndex)
		: nullptr;
	const bool bNewHasItem = Item && Item->IsValid();
	UDRItemDefinition* NewDefinition = bNewHasItem ? Item->Definition.Get() : nullptr;
	const int32 NewQuantity = bNewHasItem ? Item->Quantity : 0;

	UE_MVVM_SET_PROPERTY_VALUE(InstanceId, bNewHasItem ? Item->InstanceId : FGuid());
	UE_MVVM_SET_PROPERTY_VALUE(ItemDefinition, NewDefinition);
	UE_MVVM_SET_PROPERTY_VALUE(ItemIcon, IsValid(NewDefinition) ? NewDefinition->Icon.Get() : nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(Quantity, NewQuantity);
	UE_MVVM_SET_PROPERTY_VALUE(
		QuantityText,
		NewQuantity > 1 ? FText::AsNumber(NewQuantity) : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bHasItem, bNewHasItem);
	UE_MVVM_SET_PROPERTY_VALUE(
		bIsLocked,
		IsValid(Inventory) && Inventory->IsSlotLocked(SlotIndex));
}

void UDRInventoryViewModel::Initialize(UDRInventoryComponent* InInventoryComponent)
{
	Deinitialize();
	InventoryComponent = InInventoryComponent;
	UE_MVVM_SET_PROPERTY_VALUE(bIsLocalPlayer, true);
	UE_MVVM_SET_PROPERTY_VALUE(bIsOccupied, InventoryComponent.IsValid());
	const APlayerController* OwnerController = IsValid(InInventoryComponent)
		? Cast<APlayerController>(InInventoryComponent->GetOwner())
		: nullptr;
	const APlayerState* OwnerPlayerState = IsValid(OwnerController)
		? OwnerController->GetPlayerState<APlayerState>()
		: nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(
		PlayerName,
		IsValid(OwnerPlayerState)
			? FText::FromString(OwnerPlayerState->GetPlayerName())
			: FText::GetEmpty());

	if (!InventoryComponent.IsValid())
	{
		return;
	}

	InventoryComponent->OnInventoryChangedDelegate.AddDynamic(
		this,
		&ThisClass::HandleInventoryChanged);
	RebuildSlotEntries();
}

void UDRInventoryViewModel::Initialize(ADRPlayerState* InPlayerState, bool bInIsLocalPlayer)
{
	Deinitialize();
	PlayerState = InPlayerState;
	UE_MVVM_SET_PROPERTY_VALUE(bIsLocalPlayer, bInIsLocalPlayer);
	UE_MVVM_SET_PROPERTY_VALUE(bIsOccupied, IsValid(InPlayerState));
	UE_MVVM_SET_PROPERTY_VALUE(
		PlayerName,
		IsValid(InPlayerState) ? FText::FromString(InPlayerState->GetPlayerName()) : FText::GetEmpty());

	if (IsValid(InPlayerState))
	{
		InPlayerState->OnPublicInventoryChanged.AddDynamic(this, &ThisClass::HandleInventoryChanged);
	}

	RebuildSlotEntries();
}

void UDRInventoryViewModel::Deinitialize()
{
	if (InventoryComponent.IsValid())
	{
		InventoryComponent->OnInventoryChangedDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryChanged);
	}

	InventoryComponent.Reset();
	if (PlayerState.IsValid())
	{
		PlayerState->OnPublicInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}
	PlayerState.Reset();
	UE_MVVM_SET_PROPERTY_VALUE(
		SlotEntries,
		TArray<TObjectPtr<UDRInventorySlotEntryViewModel>>());
	UE_MVVM_SET_PROPERTY_VALUE(
		QuickSlotEntries,
		TArray<TObjectPtr<UDRInventorySlotEntryViewModel>>());
}

void UDRInventoryViewModel::HandleInventoryChanged()
{
	RefreshSlotEntries();
}

void UDRInventoryViewModel::RebuildSlotEntries()
{
	TArray<TObjectPtr<UDRInventorySlotEntryViewModel>> NewEntries;

	if (InventoryComponent.IsValid())
	{
		NewEntries.Reserve(InventoryComponent->GetMaxSlots());

		for (int32 SlotIndex = 0; SlotIndex < InventoryComponent->GetMaxSlots(); ++SlotIndex)
		{
			UDRInventorySlotEntryViewModel* Entry =
				NewObject<UDRInventorySlotEntryViewModel>(this);
			Entry->Initialize(InventoryComponent.Get(), SlotIndex);
			NewEntries.Add(Entry);
		}
	}
	else if (PlayerState.IsValid())
	{
		const TArray<FDRPublicInventorySlot>& Snapshot = PlayerState->GetPublicInventorySlots();
		NewEntries.Reserve(Snapshot.Num());
		for (int32 SlotIndex = 0; SlotIndex < Snapshot.Num(); ++SlotIndex)
		{
			UDRInventorySlotEntryViewModel* Entry = NewObject<UDRInventorySlotEntryViewModel>(this);
			Entry->Initialize(Snapshot[SlotIndex], SlotIndex);
			NewEntries.Add(Entry);
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(QuickSlotEntries, NewEntries);
	UE_MVVM_SET_PROPERTY_VALUE(SlotEntries, MoveTemp(NewEntries));
}

void UDRInventoryViewModel::RefreshSlotEntries()
{
	if (PlayerState.IsValid())
	{
		RebuildSlotEntries();
		return;
	}

	for (UDRInventorySlotEntryViewModel* Entry : SlotEntries)
	{
		if (IsValid(Entry))
		{
			Entry->Refresh();
		}
	}
}

void UDRInventoryScreenViewModel::Initialize(APlayerController* InPlayerController)
{
	UDRInventoryViewModel* NewLeftPanel = NewObject<UDRInventoryViewModel>(this);
	UDRInventoryViewModel* NewCenterPanel = NewObject<UDRInventoryViewModel>(this);
	UDRInventoryViewModel* NewRightPanel = NewObject<UDRInventoryViewModel>(this);

	ADRPlayerController* DRPlayerController = Cast<ADRPlayerController>(InPlayerController);
	ADRPlayerState* LocalPlayerState = IsValid(InPlayerController)
		? InPlayerController->GetPlayerState<ADRPlayerState>()
		: nullptr;
	if (IsValid(DRPlayerController) && IsValid(LocalPlayerState))
	{
		NewCenterPanel->Initialize(DRPlayerController->GetInventoryComponent());
		TArray<ADRPlayerState*> Teammates;
		if (AGameStateBase* GameState = InPlayerController->GetWorld()->GetGameState())
		{
			for (APlayerState* PlayerState : GameState->PlayerArray)
			{
				ADRPlayerState* Teammate = Cast<ADRPlayerState>(PlayerState);
				if (IsValid(Teammate) && Teammate != LocalPlayerState
					&& Teammate->GetTeamId() == LocalPlayerState->GetTeamId())
				{
					Teammates.Add(Teammate);
				}
			}
		}

		Teammates.Sort([](const ADRPlayerState& Left, const ADRPlayerState& Right)
		{
			return Left.GetPlayerId() < Right.GetPlayerId();
		});
		NewLeftPanel->Initialize(Teammates.IsValidIndex(0) ? Teammates[0] : nullptr, false);
		NewRightPanel->Initialize(Teammates.IsValidIndex(1) ? Teammates[1] : nullptr, false);
	}

	UE_MVVM_SET_PROPERTY_VALUE(LeftPanel, NewLeftPanel);
	UE_MVVM_SET_PROPERTY_VALUE(CenterPanel, NewCenterPanel);
	UE_MVVM_SET_PROPERTY_VALUE(RightPanel, NewRightPanel);
}
