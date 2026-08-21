#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRScoreboardWidget.generated.h"

class ADRPlayerController;
class UDRScoreboardViewModel;
class UDRScoreboardPlayerEntryViewModel;
class UDRScoreboardPlayerRowWidget;
class UVerticalBox;


/**
 * 실제 Scoreboard 화면 Widget의 C++ 부모.
 *
 * 역할:
 * 1. ScoreboardViewModel 생성
 * 2. Widget Blueprint에 ViewModel 주입
 * 3. Friendly/Enemy Entry VM 배열을 실제 Row Widget으로 생성
 */
UCLASS()
class DEEPRAIDERS_API UDRScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeScoreboard(ADRPlayerController* InPlayerController);

	/**
	 * 현재 GameState.PlayerArray를 다시 읽는다.
	 */
	void RefreshPlayers();

	/**
	 * MVVM에서 FriendlyEntries 변경 시 호출.
	 */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard|MVVM")
	void SetFriendlyEntries(const TArray<UDRScoreboardPlayerEntryViewModel*>& NewEntries);

	/**
	 * MVVM에서 EnemyEntries 변경 시 호출.
	 */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard|MVVM")
	void SetEnemyEntries(const TArray<UDRScoreboardPlayerEntryViewModel*>& NewEntries);

protected:
	virtual void NativeDestruct() override;

private:
	void RebuildEntries(UVerticalBox* Panel, const TArray<UDRScoreboardPlayerEntryViewModel*>& Entries);

	/**
	 * WBP_DRScoreboard에서 등록할 Manual ViewModel 이름.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard|MVVM")
	FName ScoreboardViewModelName = TEXT("DRScoreboardViewModel");

	/**
	 * WBP_DRScoreboardPlayerRow에서 등록할
	 * Manual ViewModel 이름.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard|MVVM")
	FName EntryViewModelName = TEXT("ScoreboardPlayerEntryViewModel");

	/**
	 * 실제 플레이어 Row Widget Blueprint.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard")
	TSubclassOf<UDRScoreboardPlayerRowWidget> PlayerRowWidgetClass;

	/**
	 * Widget Blueprint 안의 이름이 정확히
	 * FriendlyPanel 이어야 한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> FriendlyPanel;

	/**
	 * Widget Blueprint 안의 이름이 정확히
	 * EnemyPanel 이어야 한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> EnemyPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDRScoreboardViewModel> ScoreboardViewModel;
};
