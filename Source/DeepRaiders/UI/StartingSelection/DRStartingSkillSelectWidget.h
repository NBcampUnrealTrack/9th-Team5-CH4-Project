#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ListView.h"
#include "DRStartingSkillSelectWidget.generated.h"

class UButton;
class UListView;
class UTextBlock;
class UDRStartingSkillViewModel;
class UDRStartingSelectionComponent;

UCLASS()
class DEEPRAIDERS_API UDRStartingSkillListView : public UListView
{
	GENERATED_BODY()
};

UCLASS()
class DEEPRAIDERS_API UDRStartingSkillSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDRStartingSkillSelectWidget(const FObjectInitializer& ObjectInitializer);

	void InitializeSelection(
		UDRStartingSelectionComponent* InSelectionComponent);
	void DeinitializeSelection();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Starting Skill|Widget", meta = (BindWidget))
	TObjectPtr<UListView> SkillListView;

	UPROPERTY(BlueprintReadOnly, Category = "Starting Skill|Widget", meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, Category = "Starting Skill|Widget", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectionText;

private:
	void HandleSelectionGuideTextChanged(const FText& SelectionGuideText);
	void HandleSkillClicked(UObject* Item);

	UFUNCTION()
	void HandleConfirmClicked();

	UPROPERTY(EditDefaultsOnly, Category = "Starting Skill|MVVM")
	FName ViewModelName = TEXT("DRStartingSkillViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRStartingSkillViewModel> ViewModel;
};
