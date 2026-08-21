
#include "DRQuickSlotUIComponent.h"

#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DRQuickSlotWidget.h"
#include "Engine/LocalPlayer.h"

UDRQuickSlotUIComponent::UDRQuickSlotUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRQuickSlotUIComponent::BeginPlay()
{
	Super::BeginPlay();
	
	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());

	if (!IsValid(PlayerController))
	{
		UE_LOG(LogTemp, Error, TEXT("QuickSlot UI owner is not DRPlayerController: %s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	if (!PlayerController->IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();
	}

	if (!IsValid(UIManager))
	{
		return;
	}
	
	QuickSlotWidget = Cast<UDRQuickSlotWidget>(
		UIManager->PushScreen(DRGameplayTags::UI_Screen_QuickSlot));
	if (!IsValid(QuickSlotWidget))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to push QuickSlot screen"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("QuickSlot widget created: %s"), *GetNameSafe(QuickSlotWidget));
	
	// 위젯에 QuickSlot을 연결
	QuickSlotWidget->InitializeQuickSlot(PlayerController->GetQuickSlotComponent());
	
}

void UDRQuickSlotUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(UIManager))
	{
		UIManager->PopScreen(DRGameplayTags::UI_Screen_QuickSlot);
	}

	QuickSlotWidget = nullptr;
	UIManager = nullptr;
	
	Super::EndPlay(EndPlayReason);
}

