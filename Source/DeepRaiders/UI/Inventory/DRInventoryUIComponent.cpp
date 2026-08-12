
#include "DRInventoryUIComponent.h"

#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Storage/DRStorage.h"
#include "DeepRaiders/UI/Inventory/DRInventoryWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "GameFramework/PlayerState.h"

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
	
	if (IsValid(PlayerInventoryWidget))
	{
		PlayerInventoryWidget->OnEntryClickedDelegate.RemoveDynamic(this, &ThisClass::HandlePlayerEntryClicked);
		PlayerInventoryWidget->OnCloseRequestedDelegate.RemoveDynamic(this, &ThisClass::HandleCloseRequested);
		
		PlayerInventoryWidget->RemoveFromParent();
		PlayerInventoryWidget = nullptr;
	}
	
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
		UIState = EDRInventoryUIState::PlayerOnly;
		ShowPlayerInventory();
		ApplyInputMode(EDRInventoryInputMode::GameAndUI);
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
		
		CurrentStorage = NewStorage;
		
		ShowPlayerInventory();
		ShowStorageInventory(NewStorage);
		StartStorageDistanceCheck();
		ApplyInputMode(EDRInventoryInputMode::GameAndUI);
		return;
	}
	
	if (UIState == EDRInventoryUIState::PlayerAndStorage)
	{
		CloseInventoryScreen();
	}
}

void UDRInventoryUIComponent::ShowPlayerInventory()
{
	// 이미 존재하는 경우
	if (IsValid(PlayerInventoryWidget))
	{
		PlayerInventoryWidget->SetVisibility(ESlateVisibility::Visible);
		return;
	}
	
	if (!PlayerInventoryWidgetClass)
	{
		return;
	}
	
	PlayerInventoryWidget = CreateWidget<UDRInventoryWidget>(PlayerController, PlayerInventoryWidgetClass);
	if (!IsValid(PlayerInventoryWidget))
	{
		return;
	}
	
	// InventoryComponent와 Widget 연결
	PlayerInventoryWidget->InitializeInventory(PlayerController->GetInventoryComponent());
	
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
	
	// Player Inventory는 지우지 않고 캐싱
	// Visibility만 조정
	PlayerInventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
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
	
	// 오픈 도중 소유권 전환 처리 (ex: 창고 확인 중 기절)
	Storage->OnStorageOwnerChangedDelegate.AddDynamic(this, &ThisClass::HandleStorageOwnerChanged);
	
	StorageInventoryWidget = CreateWidget<UDRInventoryWidget>(PlayerController, StorageInventoryWidgetClass);
	if (!IsValid(StorageInventoryWidget))
	{
		return;
	}
	StorageInventoryWidget->InitializeInventory(Storage->GetInventoryComponent());
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
	
	if (IsValid(CurrentStorage.Get()))
	{
		// 오픈 도중 소유권 전환 처리 (ex: 창고 확인 중 기절)
		CurrentStorage->OnStorageOwnerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleStorageOwnerChanged);		
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
	else if (UIState == EDRInventoryUIState::PlayerOnly)
	{
		UDRQuickSlotComponent* QuickSlot = PlayerController->GetQuickSlotComponent();
		if (!IsValid(QuickSlot))
		{
			return;
		}
		
		UDRInventoryComponent* PlayerInventory = PlayerController->GetInventoryComponent();
		if (!IsValid(PlayerInventory))
		{
			return;
		}
		const FDRInventoryEntry* Entry = PlayerInventory->GetEntry(EntryId);
		
		QuickSlot->TryBindSelectedSlot(Entry->Definition);
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

void UDRInventoryUIComponent::HandleStorageOwnerChanged(AActor* PreviousOwner, AActor* NewOwner)
{
	if (!IsValid(PlayerController)
		|| PlayerController->GetPlayerState<APlayerState>() == NewOwner)
	{
		return ;
	}
	
	// 창고 오픈 상태
	const bool bHadStorage = UIState == EDRInventoryUIState::PlayerAndStorage;
	
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
	ApplyInputMode(EDRInventoryInputMode::GameOnly);	
}

void UDRInventoryUIComponent::ApplyInputMode(EDRInventoryInputMode InInputMode)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	switch (InInputMode)
	{
	case EDRInventoryInputMode::GameAndUI:
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	
			PlayerController->SetInputMode(InputMode);
			PlayerController->bShowMouseCursor = true;
			break;
		}
	case EDRInventoryInputMode::GameOnly:
		{
			PlayerController->SetInputMode(FInputModeGameOnly());
			PlayerController->bShowMouseCursor = false;
			break;
		}
	}
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
