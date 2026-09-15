#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRKillFeedWidget.generated.h"

class ADRPlayerController;
class UDRKillFeedViewModel;
class UDRKillFeedEntryViewModel;
class UDRKillFeedEntryWidget;
class UVerticalBox;

UCLASS()
class DEEPRAIDERS_API UDRKillFeedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeKillFeed(ADRPlayerController* InPlayerController);

	/** MVVM Entries 변경 시 WBP binding에서 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Kill Feed|MVVM")
	void SetEntries(const TArray<UDRKillFeedEntryViewModel*>& NewEntries);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void RebuildEntries(const TArray<UDRKillFeedEntryViewModel*>& Entries);

	/** WBP_KillFeed에 등록할 Manual ViewModel 이름. */
	UPROPERTY(EditDefaultsOnly, Category = "Kill Feed|MVVM")
	FName KillFeedViewModelName = TEXT("DRKillFeedViewModel");

	/** WBP_KillFeedEntry에 등록할 Manual ViewModel 이름. */
	UPROPERTY(EditDefaultsOnly, Category = "Kill Feed|MVVM")
	FName EntryViewModelName = TEXT("KillFeedEntryViewModel");

	UPROPERTY(EditDefaultsOnly, Category = "Kill Feed")
	TSubclassOf<UDRKillFeedEntryWidget> EntryWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> KillFeedPanel;

	UPROPERTY(Transient)
	TObjectPtr<UDRKillFeedViewModel> KillFeedViewModel;
};
