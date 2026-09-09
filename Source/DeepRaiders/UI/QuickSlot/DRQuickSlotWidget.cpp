// Fill out your copyright notice in the Description page of Project Settings.


#include "DRQuickSlotWidget.h"

#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/UI/ViewModel/DRQuickSlotViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// HUD의 자식으로 배치된 퀵슬롯은 생성 시 데이터만 연결한다.
	ADRPlayerController* PlayerController = GetOwningPlayer<ADRPlayerController>();
	if (IsValid(PlayerController) && PlayerController->IsLocalController())
	{
		InitializeQuickSlot(PlayerController->GetQuickSlotComponent());
	}
}

void UDRQuickSlotWidget::NativeDestruct()
{
	if (IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel->Deinitialize();
	}

	Super::NativeDestruct();
}

void UDRQuickSlotWidget::InitializeQuickSlot(UDRQuickSlotComponent* NewQuickSlotComponent)
{
	if (!IsValid(NewQuickSlotComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("QuickSlotComponent is invalid on %s"), *GetName());
		return;
	}

	if (!IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel = NewObject<UDRQuickSlotViewModel>(this);
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);

	if (!IsValid(View)
		|| !View->SetViewModel(QuickSlotViewModelName, QuickSlotViewModel))
	{
		UE_LOG(LogTemp, Error, TEXT("QuickSlotViewModel '%s' was not registered on %s"),
			*QuickSlotViewModelName.ToString(), *GetName());
		return;
	}

	QuickSlotViewModel->Initialize(NewQuickSlotComponent);
}
