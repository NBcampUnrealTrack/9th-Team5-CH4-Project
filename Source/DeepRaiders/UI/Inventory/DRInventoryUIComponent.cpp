
#include "DRInventoryUIComponent.h"

#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Storage/DRStorage.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/Inventory/DRInventoryWidget.h"
#include "DeepRaiders/UI/Inventory/DRInventoryScreenWidget.h"
#include "AbilitySystemComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "GameFramework/PlayerState.h"

UDRInventoryUIComponent::UDRInventoryUIComponent()
{
	check(true);
	
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

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();
	}
	
	//PlayerController->OnCurrentStorageChangedDelegate.AddDynamic(this, &ThisClass::HandleCurrentStorageChanged);
	
	
	// 최초 실행 초기화
	//HandleCurrentStorageChanged(PlayerController->GetCurrentStorage());
}

void UDRInventoryUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStorageDistanceCheck();
	
	if (IsValid(PlayerController))
	{
		//PlayerController->OnCurrentStorageChangedDelegate.RemoveDynamic(this, &ThisClass::HandleCurrentStorageChanged);
	}
	
	HideStorageInventory();
	HidePlayerInventory();
	
	if (IsValid(PlayerInventoryWidget))
	{
		PlayerInventoryWidget->OnEntryClickedDelegate.RemoveDynamic(this, &ThisClass::HandlePlayerEntryClicked);
		PlayerInventoryWidget->OnCloseRequestedDelegate.RemoveDynamic(this, &ThisClass::HandleCloseRequested);
		
		if (IsValid(UIManager))
		{
			UIManager->PopScreen(DRGameplayTags::UI_Screen_Inventory_Player);
		}
		else
		{
			PlayerInventoryWidget->RemoveFromParent();
		}

		PlayerInventoryWidget = nullptr;
	}

	UIManager = nullptr;
	
	Super::EndPlay(EndPlayReason);
}

void UDRInventoryUIComponent::TogglePlayerInventory()
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	// 내부 상태가 아닌 실제 화면 표시 상태를 기준으로 토글한다.
	if (IsValid(UIManager) && UIManager->IsScreenOpen(DRGameplayTags::UI_Screen_Inventory_Player))
	{
		CloseInventoryScreen();
		return;
	}

	UIState = EDRInventoryUIState::PlayerOnly;
	ShowPlayerInventory();
}

void UDRInventoryUIComponent::CloseInventory()
{
	CloseInventoryScreen();
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
		UIManager->SetManagedWidgetVisible(PlayerInventoryWidget, true);
		SetInventoryOpenTag(true);
		return;
	}
	
	if (!IsValid(UIManager))
	{
		return;
	}
	
	PlayerInventoryWidget = Cast<UDRInventoryScreenWidget>(
		UIManager->PushScreen(DRGameplayTags::UI_Screen_Inventory_Player));
	if (!IsValid(PlayerInventoryWidget))
	{
		return;
	}
	
	// Screen ViewModel이 로컬 플레이어와 팀원 패널을 구성한다.
	PlayerInventoryWidget->InitializeScreen(PlayerController);
	
	PlayerInventoryWidget->OnEntryClickedDelegate.AddDynamic(this, &ThisClass::HandlePlayerEntryClicked);
	PlayerInventoryWidget->OnCloseRequestedDelegate.AddDynamic(this, &ThisClass::HandleCloseRequested);
	SetInventoryOpenTag(true);
}

void UDRInventoryUIComponent::HidePlayerInventory()
{
	SetInventoryOpenTag(false);

	if (!IsValid(PlayerInventoryWidget))
	{
		return;
	}
	
	// Player Inventory는 지우지 않고 캐싱
	// Visibility만 조정
	UIManager->SetManagedWidgetVisible(PlayerInventoryWidget, false);
}

void UDRInventoryUIComponent::SetInventoryOpenTag(bool bIsOpen) const
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = PlayerController->GetAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(
			DRGameplayTags::State_UI_InventoryOpen,
			bIsOpen ? 1 : 0);
	}
}

void UDRInventoryUIComponent::ShowStorageInventory(ADRStorage* Storage)
{
	HideStorageInventory();
	
	if (!IsValid(Storage)
		|| !IsValid(Storage->GetInventoryComponent())
		|| !IsValid(UIManager))
	{
		return;
	}
	
	// 오픈 도중 소유권 전환 처리 (ex: 창고 확인 중 기절)
	Storage->OnStorageOwnerChangedDelegate.AddDynamic(this, &ThisClass::HandleStorageOwnerChanged);
	
	StorageInventoryWidget = Cast<UDRInventoryWidget>(
		UIManager->PushScreen(DRGameplayTags::UI_Screen_Inventory_Storage));
	if (!IsValid(StorageInventoryWidget))
	{
		return;
	}
	StorageInventoryWidget->InitializeInventory(Storage->GetInventoryComponent());
	StorageInventoryWidget->OnEntryClickedDelegate.AddDynamic(this, &ThisClass::HandleStorageEntryClicked);
	StorageInventoryWidget->OnCloseRequestedDelegate.AddDynamic(this, &ThisClass::HandleCloseRequested);
	
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
	
	if (IsValid(UIManager))
	{
		UIManager->PopScreen(DRGameplayTags::UI_Screen_Inventory_Storage);
	}
	else
	{
		StorageInventoryWidget->RemoveFromParent();
	}

	StorageInventoryWidget = nullptr;
}

void UDRInventoryUIComponent::HandlePlayerEntryClicked(FGuid InstanceId)
{
	if (UIState == EDRInventoryUIState::PlayerAndStorage)
	{
		// 추후 저장고 전송 요청 함수 호출
		return;
	}
	
	if (UIState != EDRInventoryUIState::PlayerOnly
		|| !IsValid(PlayerController))
	{
		return;
	}

	UDRInventoryComponent* PlayerInventory = PlayerController->GetInventoryComponent();
	UDRQuickSlotComponent* QuickSlot = PlayerController->GetQuickSlotComponent();
	
	if (!IsValid(QuickSlot)
		|| !IsValid(PlayerInventory))
	{
		return;
	}
	
	const int32 SlotIndex = PlayerInventory->FindSlotIndex(InstanceId);
	
	if (SlotIndex != INDEX_NONE)
	{
		QuickSlot->RequestSelectSlot(SlotIndex);
	}
}

void UDRInventoryUIComponent::HandleStorageEntryClicked(FGuid InstanceId)
{
	if (UIState == EDRInventoryUIState::PlayerAndStorage)
	{
		// 추후 StorageToPlayer 전송 함수 추가
	}
}

void UDRInventoryUIComponent::HandleCloseRequested()
{
	const bool bHadStorage = UIState == EDRInventoryUIState::PlayerAndStorage;
	
	CloseInventoryScreen();
	if (bHadStorage)
	{
		//PlayerController->RequestCloseStorage();
	}
}

void UDRInventoryUIComponent::HandleStorageOwnerChanged(AActor* PreviousOwner, AActor* NewOwner)
{
	if (!IsValid(PlayerController)
		|| PlayerController->GetPawn() == NewOwner)
	{
		return ;
	}
	
	// 창고 오픈 상태
	const bool bHadStorage = UIState == EDRInventoryUIState::PlayerAndStorage;
	
	if (bHadStorage)
	{
		//PlayerController->RequestCloseStorage();
	}
}

void UDRInventoryUIComponent::CloseInventoryScreen()
{
	StopStorageDistanceCheck();
	HidePlayerInventory();
	HideStorageInventory();
	
	UIState = EDRInventoryUIState::Closed;
}

void UDRInventoryUIComponent::StartStorageDistanceCheck()
{
	if (UWorld* World = GetWorld(); IsValid(PlayerController) && IsValid(World))
	{
		//World->GetTimerManager().SetTimer(StorageDistanceTimerHandle, this, &ThisClass::CheckStorageDistance
		//	, PlayerController->GetStorageDistanceCheckInterval(), true);
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
	//ADRStorage* Storage = PlayerController->GetCurrentStorage();
	
	// 플레이어가 창고와 상호작용 가능한 거리인지 체크
	//if (!PlayerController->IsStorageWithinInteractionRange(Storage))
	{
		CloseInventoryScreen();
		//PlayerController->RequestCloseStorage();
	}
}
