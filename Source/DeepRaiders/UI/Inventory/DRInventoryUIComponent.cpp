
#include "DRInventoryUIComponent.h"

#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Storage/DRStorage.h"
#include "DeepRaiders/UI/Inventory/DRInventoryWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"

UDRInventoryUIComponent::UDRInventoryUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRInventoryUIComponent::BeginPlay()
{
	Super::BeginPlay();
	
	PlayerController = Cast<ADRPlayerController>(GetOwner());
	
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController())
	{
		return;
	}
	
	PlayerController->OnCurrentStorageChangedDelegate.AddDynamic(this, &ThisClass::HandleCurrentStorageChanged);
	
	// 최초 실행 초기화
	HandleCurrentStorageChanged(PlayerController->GetCurrentStorage());
}

void UDRInventoryUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStorageDistanceCheck();
	
	if (IsValid(PlayerController))
	{
		PlayerController->OnCurrentStorageChangedDelegate.RemoveDynamic(this, &ThisClass::HandleCurrentStorageChanged);
	}
	
	HideStorageInventory();
	HidePlayerInventory();
	
	Super::EndPlay(EndPlayReason);
}

void UDRInventoryUIComponent::TogglePlayerInventory()
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	switch (UIState)
	{
	case EDRInventoryUIState::Closed:
		UIState =  EDRInventoryUIState::PlayerOnly;
		ShowPlayerInventory();
		ApplyInputMode();
		break;
		
	case EDRInventoryUIState::PlayerOnly:
		CloseInventoryScreen();
		break;
		
	case EDRInventoryUIState::PlayerAndStorage:
		CloseInventoryScreen();
		PlayerController->RequestCloseStorage();
		break;
	}
}

void UDRInventoryUIComponent::HandleCurrentStorageChanged(ADRStorage* NewStorage)
{
	if (IsValid(NewStorage))
	{
		UIState = EDRInventoryUIState::PlayerAndStorage;
		
		ShowPlayerInventory();
		ShowStorageInventory(NewStorage);
		StartStorageDistanceCheck();
		ApplyInputMode();
		return;
	}
	
	if (UIState == EDRInventoryUIState::PlayerAndStorage)
	{
		CloseInventoryScreen();
	}
}

void UDRInventoryUIComponent::ShowPlayerInventory()
{
	if (IsValid(PlayerInventoryWidget)
		|| !PlayerInventoryWidgetClass)
	{
		return;
	}
	
	PlayerInventoryWidget = CreateWidget<UDRInventoryWidget>(PlayerController, PlayerInventoryWidgetClass);
	if (!IsValid(PlayerInventoryWidget))
	{
		return;
	}
	
	// InventoryComponent와 Widget 연결
	PlayerInventoryWidget->InitializeInventory(PlayerController->GetQuickSlotInventoryComponent());
	
	PlayerInventoryWidget->OnEntryClickedDelegate.AddDynamic(this, &ThisClass::HandlePlayerEntryClicked);
	PlayerInventoryWidget->OnCloseRequestedDelegate.AddDynamic(this, &ThisClass::HandleCloseRequested);
	
	// UI 순서 임의로 지정, 신다인 테스트
	PlayerInventoryWidget->AddToViewport(5);
}

void UDRInventoryUIComponent::HidePlayerInventory()
{
	if (!IsValid(PlayerInventoryWidget))
	{
		return;
	}
	
	PlayerInventoryWidget->OnEntryClickedDelegate.RemoveDynamic(this, &ThisClass::HandlePlayerEntryClicked);
	PlayerInventoryWidget->OnCloseRequestedDelegate.RemoveDynamic(this, &ThisClass::HandleCloseRequested);
	
	PlayerInventoryWidget->RemoveFromParent();
	PlayerInventoryWidget = nullptr;
}

void UDRInventoryUIComponent::ShowStorageInventory(ADRStorage* Storage)
{
	HideStorageInventory();
	
	if (!IsValid(Storage)
		|| !IsValid(Storage->GetInventoryComponent())
		|| !StorageInventoryWidgetClass)
	{
		return;
	}
	
	StorageInventoryWidget = CreateWidget<UDRInventoryWidget>(PlayerController, StorageInventoryWidgetClass);
	
	if (!IsValid(StorageInventoryWidget))
	{
		return;
	}
	
	StorageInventoryWidget->OnEntryClickedDelegate.AddDynamic(this, &ThisClass::HandleStorageEntryClicked);
	StorageInventoryWidget->OnCloseRequestedDelegate.AddDynamic(this, &ThisClass::HandleCloseRequested);
	
	// UI 순서 임의로 지정, 신다인 테스트
	StorageInventoryWidget->AddToViewport(10);
}

void UDRInventoryUIComponent::HideStorageInventory()
{
	if (!IsValid(StorageInventoryWidget))
	{
		return;
	}
	
	StorageInventoryWidget->OnEntryClickedDelegate.RemoveDynamic(this, &ThisClass::HandleStorageEntryClicked);
	StorageInventoryWidget->OnCloseRequestedDelegate.RemoveDynamic(this, &ThisClass::HandleCloseRequested);
	
	StorageInventoryWidget->RemoveFromParent();
	StorageInventoryWidget = nullptr;
}

void UDRInventoryUIComponent::HandlePlayerEntryClicked(FGuid EntryId)
{
	if (UIState == EDRInventoryUIState::PlayerAndStorage)
	{
		PlayerController->RequestTransferStorageItem(EDRStorageTransferDirection::PlayerToStorage, EntryId);
	}
}

void UDRInventoryUIComponent::HandleStorageEntryClicked(FGuid EntryId)
{
	if (UIState == EDRInventoryUIState::PlayerAndStorage)
	{
		PlayerController->RequestTransferStorageItem(EDRStorageTransferDirection::StorageToPlayer, EntryId);
	}
}

void UDRInventoryUIComponent::HandleCloseRequested()
{
	const bool bHadStorage = UIState == EDRInventoryUIState::PlayerAndStorage;
	
	CloseInventoryScreen();
	if (bHadStorage)
	{
		PlayerController->RequestCloseStorage();
	}
}

void UDRInventoryUIComponent::CloseInventoryScreen()
{
	StopStorageDistanceCheck();
	HidePlayerInventory();
	HideStorageInventory();
	
	UIState = EDRInventoryUIState::Closed;
	ApplyInputMode();	
}

void UDRInventoryUIComponent::ApplyInputMode()
{
	if (!IsValid(PlayerController))
	{
		return;
	}
	
	PlayerController->FlushPressedKeys();
	
	if (UIState == EDRInventoryUIState::Closed)
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
		return;
	}
	
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = true;
}

void UDRInventoryUIComponent::StartStorageDistanceCheck()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(StorageDistanceTimerHandle, this, &ThisClass::CheckStorageDistance
			, StorageDistanceCheckInterval, true);
	}
}

void UDRInventoryUIComponent::StopStorageDistanceCheck()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StorageDistanceTimerHandle);
	}
}

void UDRInventoryUIComponent::CheckStorageDistance()
{
	ADRStorage* Storage = PlayerController->GetCurrentStorage();
	
	// 플레이어가 창고와 상호작용 가능한 거리인지 체크
	if (!PlayerController->IsStorageWithinInteractionRange(Storage))
	{
		CloseInventoryScreen();
		PlayerController->RequestCloseStorage();
	}
}
