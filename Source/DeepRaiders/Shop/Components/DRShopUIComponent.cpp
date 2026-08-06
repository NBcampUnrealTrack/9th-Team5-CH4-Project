#include "DRShopUIComponent.h"

#include "DRInteractionComponent.h"
#include "Blueprint/UserWidget.h"
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

	ShopWidget = CreateWidget<UUserWidget>(
		PlayerController,
		ShopWidgetClass);

	if (IsValid(ShopWidget))
	{
		ShopWidget->AddToViewport();
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

	ShopWidget->RemoveFromParent();
	ShopWidget = nullptr;
}
