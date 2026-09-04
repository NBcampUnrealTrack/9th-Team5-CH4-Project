#include "DRHUDUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/ViewModel/DRHUDViewModel.h"
#include "Engine/LocalPlayer.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

namespace DRHUDUI
{
	int32 RegisterViewModelRecursively(
		UUserWidget* Widget,
		FName ViewModelName,
		UDRHUDViewModel* ViewModel,
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
			RegisteredCount += View->SetViewModel(ViewModelName, ViewModel) ? 1 : 0;
		}

		TArray<UWidget*> ChildWidgets;
		Widget->WidgetTree->GetAllWidgets(ChildWidgets);
		for (UWidget* ChildWidget : ChildWidgets)
		{
			RegisteredCount += RegisterViewModelRecursively(
				Cast<UUserWidget>(ChildWidget),
				ViewModelName,
				ViewModel,
				VisitedWidgets);
		}

		return RegisteredCount;
	}
}

UDRHUDUIComponent::UDRHUDUIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDRHUDUIComponent::BeginPlay()
{
	Super::BeginPlay();

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();
	}

	const UDRUIConfig* UIConfig = IsValid(UIManager) ? UIManager->GetUIConfig() : nullptr;
	if (!IsValid(UIConfig))
	{
		return;
	}

	HUDWidget = UIManager->PushScreen(DRGameplayTags::UI_Screen_HUD);
	if (!IsValid(HUDWidget))
	{
		return;
	}

	HUDViewModel = NewObject<UDRHUDViewModel>(this);
	TSet<UUserWidget*> VisitedWidgets;
	const int32 RegisteredViewCount = DRHUDUI::RegisterViewModelRecursively(
		HUDWidget,
		UIConfig->HUDViewModelName,
		HUDViewModel,
		VisitedWidgets);
	if (RegisteredViewCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("HUD ViewModels were not registered on %s"),
			*GetNameSafe(HUDWidget));
		UIManager->ReleaseManagedWidget(HUDWidget);
		HUDWidget = nullptr;
		HUDViewModel = nullptr;
		return;
	}

	RefreshPlayerCharacter();
	SetComponentTickEnabled(true);
}

void UDRHUDUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(HUDViewModel))
	{
		HUDViewModel->Deinitialize();
	}

	if (IsValid(UIManager))
	{
		UIManager->PopScreen(DRGameplayTags::UI_Screen_HUD);
	}
	else if (IsValid(HUDWidget))
	{
		HUDWidget->RemoveFromParent();
	}

	HUDWidget = nullptr;
	HUDViewModel = nullptr;
	UIManager = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UDRHUDUIComponent::RefreshPlayerCharacter()
{
	if (!IsValid(HUDViewModel))
	{
		return;
	}

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	ADRPlayerCharacter* PlayerCharacter = IsValid(PlayerController)
		? Cast<ADRPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;

	HUDViewModel->Initialize(PlayerCharacter);
}

void UDRHUDUIComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsValid(HUDViewModel))
	{
		HUDViewModel->TickGaugeInterpolation(DeltaTime);
	}
}

