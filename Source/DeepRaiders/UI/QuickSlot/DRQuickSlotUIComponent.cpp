
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
	
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !QuickSlotWidgetClass)
	{
		return;
	}
	
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

