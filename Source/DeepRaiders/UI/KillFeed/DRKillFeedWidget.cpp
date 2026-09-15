#include "DRKillFeedWidget.h"

#include "Components/VerticalBox.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/KillFeed/DRKillFeedEntryWidget.h"
#include "DeepRaiders/UI/ViewModel/DRKillFeedViewModel.h"

void UDRKillFeedWidget::InitializeKillFeed(
	ADRPlayerController* InPlayerController)
{
	if (!IsValid(InPlayerController))
	{
		return;
	}

	if (!IsValid(KillFeedViewModel))
	{
		KillFeedViewModel = NewObject<UDRKillFeedViewModel>(this);
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!IsValid(View))
	{
		UE_LOG(LogTemp, Error, TEXT("KillFeed MVVM View is invalid. Widget=%s"), *GetName());
		return;
	}

	if (!View->SetViewModel(KillFeedViewModelName, KillFeedViewModel))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("KillFeed ViewModel '%s' was not registered on %s"),
			*KillFeedViewModelName.ToString(),
			*GetName());
		return;
	}

	KillFeedViewModel->Initialize(InPlayerController);
}

void UDRKillFeedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeKillFeed(
		Cast<ADRPlayerController>(GetOwningPlayer()));
}

void UDRKillFeedWidget::NativeDestruct()
{
	if (IsValid(KillFeedViewModel))
	{
		KillFeedViewModel->Deinitialize();
		KillFeedViewModel = nullptr;
	}

	Super::NativeDestruct();
}

void UDRKillFeedWidget::SetEntries(
	const TArray<UDRKillFeedEntryViewModel*>& NewEntries)
{
	RebuildEntries(NewEntries);
}

void UDRKillFeedWidget::RebuildEntries(
	const TArray<UDRKillFeedEntryViewModel*>& Entries)
{
	if (!IsValid(KillFeedPanel))
	{
		UE_LOG(LogTemp, Error, TEXT("KillFeedPanel is invalid."));
		return;
	}

	if (!EntryWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("KillFeed EntryWidgetClass is not configured."));
		return;
	}

	KillFeedPanel->ClearChildren();

	for (UDRKillFeedEntryViewModel* EntryViewModel : Entries)
	{
		if (!IsValid(EntryViewModel))
		{
			continue;
		}

		UDRKillFeedEntryWidget* EntryWidget =
			CreateWidget<UDRKillFeedEntryWidget>(GetOwningPlayer(), EntryWidgetClass);

		if (!IsValid(EntryWidget))
		{
			continue;
		}

		UMVVMView* EntryView = UMVVMSubsystem::GetViewFromUserWidget(EntryWidget);
		if (!IsValid(EntryView))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("KillFeed Entry MVVM View is invalid. Widget=%s"),
				*GetNameSafe(EntryWidget));
			continue;
		}

		if (!EntryView->SetViewModel(EntryViewModelName, EntryViewModel))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("KillFeed Entry ViewModel '%s' was not registered on %s"),
				*EntryViewModelName.ToString(),
				*GetNameSafe(EntryWidget));
			continue;
		}

		KillFeedPanel->AddChildToVerticalBox(EntryWidget);
	}
}
