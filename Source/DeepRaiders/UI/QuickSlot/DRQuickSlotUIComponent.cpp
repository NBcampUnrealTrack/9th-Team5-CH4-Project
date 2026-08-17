
#include "DRQuickSlotUIComponent.h"

#include "DeepRaiders/Player/DRPlayerController.h"
#include "DRQuickSlotWidget.h"

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

	if (!QuickSlotWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("QuickSlotWidgetClass is not set on %s"),
			*GetNameSafe(PlayerController));
		return;
	}
	
	QuickSlotWidget = CreateWidget<UDRQuickSlotWidget>(PlayerController, QuickSlotWidgetClass);
	if (!IsValid(QuickSlotWidget))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create QuickSlot widget class: %s"),
			*GetNameSafe(QuickSlotWidgetClass));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("QuickSlot widget created: %s"), *GetNameSafe(QuickSlotWidget));
	
	// 위젯에 QuickSlot을 연결
	QuickSlotWidget->InitializeQuickSlot(PlayerController->GetQuickSlotComponent());
	
	// UI 순서 임의로 지정, 신다인 테스트
	QuickSlotWidget->AddToViewport(0);
}

void UDRQuickSlotUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(QuickSlotWidget))
	{
		QuickSlotWidget->RemoveFromParent();
		QuickSlotWidget = nullptr;
	}
	
	Super::EndPlay(EndPlayReason);
}

