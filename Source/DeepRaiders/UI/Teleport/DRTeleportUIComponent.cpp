#include "DRTeleportUIComponent.h"

#include "DeepRaiders/Player/Components/DRTeleportComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Teleport/DRTeleportPoint.h"
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/Teleport/DRTeleportSelectWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"

UDRTeleportUIComponent::UDRTeleportUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRTeleportUIComponent::BeginPlay()
{
	Super::BeginPlay();

	PlayerController = Cast<ADRPlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();
	}

	RefreshTeleportComponentBinding();
}

void UDRTeleportUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindTeleportComponent(nullptr);
	CloseTeleportSelectWidget();
	UIManager = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UDRTeleportUIComponent::RefreshTeleportComponentBinding()
{
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	APawn* ControlledPawn = PlayerController->GetPawn();
	UDRTeleportComponent* TeleportComponent = IsValid(ControlledPawn) ? ControlledPawn->FindComponentByClass<UDRTeleportComponent>() : nullptr;
	BindTeleportComponent(TeleportComponent);
}

void UDRTeleportUIComponent::BindTeleportComponent(UDRTeleportComponent* NewTeleportComponent)
{
	if (BoundTeleportComponent == NewTeleportComponent)
	{
		return;
	}

	if (IsValid(BoundTeleportComponent))
	{
		BoundTeleportComponent->OnTeleportUseRequested.RemoveDynamic(this, &ThisClass::HandleTeleportUseRequested);
	}

	BoundTeleportComponent = NewTeleportComponent;

	if (IsValid(BoundTeleportComponent))
	{
		BoundTeleportComponent->OnTeleportUseRequested.AddUniqueDynamic(this, &ThisClass::HandleTeleportUseRequested);
	}
}

void UDRTeleportUIComponent::CloseTeleportSelectWidget()
{
	if (IsValid(TeleportSelectWidget))
	{
		TeleportSelectWidget->OnCloseRequested.RemoveDynamic(
			this,
			&ThisClass::HandleTeleportCloseRequested);

		if (IsValid(UIManager))
		{
			UIManager->ReleaseManagedWidget(TeleportSelectWidget);
		}
		else
		{
			TeleportSelectWidget->RemoveFromParent();
		}

		TeleportSelectWidget = nullptr;
	}
}

void UDRTeleportUIComponent::HandleTeleportUseRequested(ADRTeleportPoint* CurrentTeleportPoint)
{
	const UDRUIConfig* UIConfig = IsValid(UIManager) ? UIManager->GetUIConfig() : nullptr;
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController()
		|| !IsValid(CurrentTeleportPoint) || !IsValid(UIConfig)
		|| !UIConfig->TeleportSelectWidgetClass)
	{
		return;
	}

	CloseTeleportSelectWidget();

	TeleportSelectWidget = Cast<UDRTeleportSelectWidget>(
		UIManager->CreateManagedWidget(
			UIConfig->TeleportSelectWidgetClass,
			UIConfig->TeleportLayer));
	if (!IsValid(TeleportSelectWidget))
	{
		return;
	}

	TeleportSelectWidget->InitializeRegisteredTeleportList(INDEX_NONE, CurrentTeleportPoint);
	TeleportSelectWidget->OnCloseRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandleTeleportCloseRequested);
}

void UDRTeleportUIComponent::HandleTeleportCloseRequested()
{
	CloseTeleportSelectWidget();
}
