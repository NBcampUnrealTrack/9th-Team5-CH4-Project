#include "DRHUDUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/Perk/DRPerkWidget.h"
#include "DeepRaiders/UI/ViewModel/DRHUDViewModel.h"
#include "Engine/LocalPlayer.h"
#include "MVVMSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "View/MVVMView.h"

UDRHUDUIComponent::UDRHUDUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(HUDWidget);
	if (!IsValid(View) || !View->SetViewModel(UIConfig->HUDViewModelName, HUDViewModel))
	{
		UE_LOG(LogTemp, Error, TEXT("HUD ViewModel '%s' was not registered on %s"),
			*UIConfig->HUDViewModelName.ToString(), *GetNameSafe(HUDWidget));
		UIManager->ReleaseManagedWidget(HUDWidget);
		HUDWidget = nullptr;
		HUDViewModel = nullptr;
		return;
	}

	RefreshPlayerCharacter();
	RefreshPerks();
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
	PerkWidget = nullptr;
	UIManager = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UDRHUDUIComponent::RefreshPlayerCharacter()
{
	if (!IsValid(HUDViewModel))
	{
		return;
	}

	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	ADRPlayerCharacter* PlayerCharacter = IsValid(PlayerController)
		? Cast<ADRPlayerCharacter>(PlayerController->GetPawn())
		: nullptr;

	HUDViewModel->Initialize(PlayerCharacter);
}

void UDRHUDUIComponent::RefreshPerks()
{
	CachePerkWidget();

	if (!IsValid(PerkWidget))
	{
		return;
	}

	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	ADRPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADRPlayerState>()
		: nullptr;
	UDRPerkComponent* PerkComponent = IsValid(PlayerState)
		? PlayerState->GetPerkComponent()
		: nullptr;

	PerkWidget->InitializePerks(PerkComponent);
}

void UDRHUDUIComponent::CachePerkWidget()
{
	if (IsValid(PerkWidget)
		|| !IsValid(HUDWidget)
		|| !IsValid(HUDWidget->WidgetTree))
	{
		return;
	}

	TArray<UWidget*> Widgets;
	HUDWidget->WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		if (UDRPerkWidget* FoundPerkWidget = Cast<UDRPerkWidget>(Widget))
		{
			PerkWidget = FoundPerkWidget;
			return;
		}
	}
}
