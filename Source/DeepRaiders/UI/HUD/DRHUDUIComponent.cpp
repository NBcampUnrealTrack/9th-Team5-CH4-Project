#include "DRHUDUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/ViewModel/DRHUDViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

UDRHUDUIComponent::UDRHUDUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRHUDUIComponent::BeginPlay()
{
	Super::BeginPlay();

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController() || !HUDWidgetClass)
	{
		return;
	}

	HUDWidget = CreateWidget<UUserWidget>(PlayerController, HUDWidgetClass);
	if (!IsValid(HUDWidget))
	{
		return;
	}

	HUDViewModel = NewObject<UDRHUDViewModel>(this);
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(HUDWidget);
	if (!IsValid(View) || !View->SetViewModel(HUDViewModelName, HUDViewModel))
	{
		UE_LOG(LogTemp, Error, TEXT("HUD ViewModel '%s' was not registered on %s"),
			*HUDViewModelName.ToString(), *GetNameSafe(HUDWidget));
		HUDWidget = nullptr;
		HUDViewModel = nullptr;
		return;
	}

	RefreshPlayerCharacter();
	HUDWidget->AddToViewport(0);
}

void UDRHUDUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(HUDViewModel))
	{
		HUDViewModel->Deinitialize();
	}

	if (IsValid(HUDWidget))
	{
		HUDWidget->RemoveFromParent();
	}

	HUDWidget = nullptr;
	HUDViewModel = nullptr;
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
