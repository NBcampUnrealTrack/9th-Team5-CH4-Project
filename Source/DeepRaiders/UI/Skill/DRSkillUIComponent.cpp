#include "DRSkillUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/ViewModel/DRSkillViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

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
	TArray<UWidget*> Widgets;
	HUDWidget->WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		UUserWidget* UserWidget = Cast<UUserWidget>(Widget);
		UMVVMView* View = IsValid(UserWidget)
			? UMVVMSubsystem::GetViewFromUserWidget(UserWidget)
			: nullptr;

		if (IsValid(View)
			&& View->SetViewModel(TEXT("DRSkillViewModel"), SkillViewModel))
		{
			return true;
		}
	}

	return false;
}
