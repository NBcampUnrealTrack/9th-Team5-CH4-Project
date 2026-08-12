#include "DRTeleportUIComponent.h"

#include "DeepRaiders/Player/Components/DRTeleportComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Teleport/DRTeleportPoint.h"
#include "DeepRaiders/UI/Teleport/DRTeleportSelectWidget.h"
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

	RefreshTeleportComponentBinding();
}

void UDRTeleportUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindTeleportComponent(nullptr);
	CloseTeleportSelectWidget();
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
		TeleportSelectWidget->RemoveFromParent();
		TeleportSelectWidget = nullptr;
	}
}

void UDRTeleportUIComponent::HandleTeleportUseRequested(ADRTeleportPoint* CurrentTeleportPoint)
{
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController() || !IsValid(CurrentTeleportPoint) || !TeleportSelectWidgetClass)
	{
		return;
	}

	CloseTeleportSelectWidget();

	TeleportSelectWidget = CreateWidget<UDRTeleportSelectWidget>(PlayerController, TeleportSelectWidgetClass);
	if (!IsValid(TeleportSelectWidget))
	{
		return;
	}

	TeleportSelectWidget->InitializeRegisteredTeleportList(INDEX_NONE, CurrentTeleportPoint);
	TeleportSelectWidget->AddToViewport(10);
}
