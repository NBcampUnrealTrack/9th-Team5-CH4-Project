#include "DRScoreboardWidget.h"

#include "Components/VerticalBox.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/ViewModel/DRScoreboardViewModel.h"
#include "DeepRaiders/UI/Scoreboard/DRScoreboardPlayerRowWidget.h"

void UDRScoreboardWidget::InitializeScoreboard(
	ADRPlayerController* InPlayerController)
{
	if (!IsValid(InPlayerController))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Scoreboard PlayerController is invalid."));

		return;
	}

	if (!IsValid(ScoreboardViewModel))
	{
		ScoreboardViewModel =
			NewObject<UDRScoreboardViewModel>(this);
	}

	UMVVMView* View =
		UMVVMSubsystem::GetViewFromUserWidget(this);

	if (!IsValid(View))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Scoreboard MVVM View is invalid. "
				"Widget=%s"),
			*GetName());

		return;
	}

	if (!View->SetViewModel(
		ScoreboardViewModelName,
		ScoreboardViewModel))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Scoreboard ViewModel '%s' "
				"was not registered on %s"),
			*ScoreboardViewModelName.ToString(),
			*GetName());

		return;
	}

	ScoreboardViewModel->Initialize(
		InPlayerController);
}

void UDRScoreboardWidget::NativeDestruct()
{
	if (IsValid(ScoreboardViewModel))
	{
		ScoreboardViewModel->Deinitialize();
		ScoreboardViewModel = nullptr;
	}

	Super::NativeDestruct();
}

void UDRScoreboardWidget::RefreshPlayers()
{
	if (IsValid(ScoreboardViewModel))
	{
		ScoreboardViewModel->RefreshPlayers();
	}
}

void UDRScoreboardWidget::SetFriendlyEntries(
	const TArray<UDRScoreboardPlayerEntryViewModel*>& NewEntries)
{
	RebuildEntries(
		FriendlyPanel,
		NewEntries);
}

void UDRScoreboardWidget::SetEnemyEntries(
	const TArray<UDRScoreboardPlayerEntryViewModel*>& NewEntries)
{
	RebuildEntries(
		EnemyPanel,
		NewEntries);
}

void UDRScoreboardWidget::RebuildEntries(
	UVerticalBox* Panel,
	const TArray<UDRScoreboardPlayerEntryViewModel*>& Entries)
{
	if (!IsValid(Panel))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Scoreboard Panel is invalid."));

		return;
	}

	if (!PlayerRowWidgetClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Scoreboard PlayerRowWidgetClass "
				"is not configured."));

		return;
	}

	/*
	 * 3v3이므로 그냥 전부 지우고 다시 만드는 게
	 * 제일 단순하고 충분히 싸다.
	 */
	Panel->ClearChildren();

	for (UDRScoreboardPlayerEntryViewModel* EntryViewModel
		: Entries)
	{
		if (!IsValid(EntryViewModel))
		{
			continue;
		}

		UDRScoreboardPlayerRowWidget* RowWidget =
			CreateWidget<UDRScoreboardPlayerRowWidget>(
				GetOwningPlayer(),
				PlayerRowWidgetClass);

		if (!IsValid(RowWidget))
		{
			continue;
		}

		UMVVMView* EntryView =
			UMVVMSubsystem::GetViewFromUserWidget(
				RowWidget);

		if (!IsValid(EntryView))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"Scoreboard Row MVVM View "
					"is invalid. Widget=%s"),
				*GetNameSafe(RowWidget));

			continue;
		}

		if (!EntryView->SetViewModel(
			EntryViewModelName,
			EntryViewModel))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"Scoreboard Entry ViewModel "
					"'%s' was not registered on %s"),
				*EntryViewModelName.ToString(),
				*GetNameSafe(RowWidget));

			continue;
		}

		Panel->AddChildToVerticalBox(
			RowWidget);
	}
}