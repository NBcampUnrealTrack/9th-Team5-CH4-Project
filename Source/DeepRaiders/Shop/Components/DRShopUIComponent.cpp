#include "DRShopUIComponent.h"

#include "DRInteractionComponent.h"
#include "DeepRaiders/UI/Shop/DRShopWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UDRShopUIComponent::UDRShopUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRShopUIComponent::BeginPlay()
{
	Super::BeginPlay();

	InteractionComponent =
		GetOwner()->FindComponentByClass<UDRInteractionComponent>();

	if (!IsValid(InteractionComponent))
	{
		return;
	}

	InteractionComponent->OnInteractionEntered.AddDynamic(
		this,
		&ThisClass::HandleInteractionEntered);
	InteractionComponent->OnInteractionExited.AddDynamic(
		this,
		&ThisClass::HandleInteractionExited);
}

void UDRShopUIComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(InteractionComponent))
	{
		InteractionComponent->OnInteractionEntered.RemoveDynamic(
			this,
			&ThisClass::HandleInteractionEntered);
		InteractionComponent->OnInteractionExited.RemoveDynamic(
			this,
			&ThisClass::HandleInteractionExited);
	}

	HideShopWidget();
	Super::EndPlay(EndPlayReason);
}

void UDRShopUIComponent::HandleInteractionEntered(APawn* Interactor)
{
	if (!IsValid(Interactor) || !Interactor->IsLocallyControlled()
		|| IsValid(ShopWidget) || !ShopWidgetClass)
	{
		return;
	}

	APlayerController* PlayerController =
		Cast<APlayerController>(Interactor->GetController());

	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	ShopWidget = CreateWidget<UDRShopWidget>(
		PlayerController,
		ShopWidgetClass);

	if (IsValid(ShopWidget))
	{
		ShopWidget->OnCloseRequested.AddDynamic(
			this,
			&ThisClass::HideShopWidget);
		ShopWidget->AddToViewport();

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ShopWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(
			EMouseLockMode::DoNotLock);
		PlayerController->FlushPressedKeys();
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}
}

void UDRShopUIComponent::HandleInteractionExited(APawn* Interactor)
{
	if (IsValid(Interactor) && Interactor->IsLocallyControlled())
	{
		HideShopWidget();
	}
}

void UDRShopUIComponent::HideShopWidget()
{
	if (!IsValid(ShopWidget))
	{
		return;
	}

	APlayerController* PlayerController = ShopWidget->GetOwningPlayer();

	ShopWidget->OnCloseRequested.RemoveDynamic(
		this,
		&ThisClass::HideShopWidget);
	ShopWidget->RemoveFromParent();
	ShopWidget = nullptr;

	if (IsValid(PlayerController))
	{
		PlayerController->FlushPressedKeys();
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
	}
}
