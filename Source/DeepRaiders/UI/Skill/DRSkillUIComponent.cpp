#include "DRSkillUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/ViewModel/DRSkillViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

namespace DRSkillUI
{
	int32 RegisterViewModelRecursively(
		UUserWidget* Widget,
		UDRSkillViewModel* ViewModel,
		TSet<UUserWidget*>& VisitedWidgets)
	{
		if (!IsValid(Widget) || VisitedWidgets.Contains(Widget))
		{
			return 0;
		}

		VisitedWidgets.Add(Widget);
		int32 RegisteredCount = 0;
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget))
		{
			RegisteredCount += View->SetViewModel(TEXT("DRSkillViewModel"), ViewModel) ? 1 : 0;
		}

		TArray<UWidget*> ChildWidgets;
		Widget->WidgetTree->GetAllWidgets(ChildWidgets);
		for (UWidget* ChildWidget : ChildWidgets)
		{
			RegisteredCount += RegisterViewModelRecursively(
				Cast<UUserWidget>(ChildWidget),
				ViewModel,
				VisitedWidgets);
		}

		return RegisteredCount;
	}
}

UDRSkillUIComponent::UDRSkillUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRSkillUIComponent::Initialize(UUserWidget* InHUDWidget)
{
	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsValid(InHUDWidget))
	{
		return;
	}

	SkillViewModel = NewObject<UDRSkillViewModel>(this);
	if (!RegisterViewModel(InHUDWidget))
	{
		UE_LOG(LogTemp, Error, TEXT("Skill ViewModel was not registered on %s"),
			*GetNameSafe(InHUDWidget));
		SkillViewModel = nullptr;
		return;
	}

	RefreshPlayerCharacter();
}

void UDRSkillUIComponent::RefreshPlayerCharacter()
{
	if (!IsValid(SkillViewModel))
	{
		return;
	}

	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	ADRPlayerCharacter* PlayerCharacter = IsValid(PlayerController)
		? Cast<ADRPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
	SkillViewModel->Initialize(PlayerCharacter);
}

void UDRSkillUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(SkillViewModel))
	{
		SkillViewModel->Deinitialize();
	}

	SkillViewModel = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool UDRSkillUIComponent::RegisterViewModel(UUserWidget* HUDWidget)
{
	TSet<UUserWidget*> VisitedWidgets;
	return DRSkillUI::RegisterViewModelRecursively(
		HUDWidget,
		SkillViewModel,
		VisitedWidgets) > 0;
}
