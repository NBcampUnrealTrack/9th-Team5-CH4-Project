#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRScoreboardWidget.generated.h"

class ADRPlayerController;
class UDRScoreboardViewModel;
class UDRScoreboardPlayerEntryViewModel;
class UDRScoreboardPlayerRowWidget;
class UVerticalBox;

UCLASS()
class DEEPRAIDERS_API UDRScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeScoreboard(ADRPlayerController* InPlayerController);

	void RefreshPlayers();

	UFUNCTION(BlueprintCallable, Category = "Scoreboard|MVVM")
	void SetBlueTeamEntries(const TArray<UDRScoreboardPlayerEntryViewModel*>& NewEntries);

	UFUNCTION(BlueprintCallable, Category = "Scoreboard|MVVM")
	void SetRedTeamEntries(const TArray<UDRScoreboardPlayerEntryViewModel*>& NewEntries);

protected:
	virtual void NativeDestruct() override;

private:
	void RebuildEntries(UVerticalBox* Panel, const TArray<UDRScoreboardPlayerEntryViewModel*>& Entries);

	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard|MVVM")
	FName ScoreboardViewModelName = TEXT("DRScoreboardViewModel");

	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard|MVVM")
	FName EntryViewModelName = TEXT("ScoreboardPlayerEntryViewModel");

	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard")
	TSubclassOf<UDRScoreboardPlayerRowWidget> PlayerRowWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> BlueTeamPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> RedTeamPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDRScoreboardViewModel> ScoreboardViewModel;
};
