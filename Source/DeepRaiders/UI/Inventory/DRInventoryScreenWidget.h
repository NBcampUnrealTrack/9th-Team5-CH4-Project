#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRInventoryScreenWidget.generated.h"

class UButton;
class APlayerController;
class UDRInventoryViewModel;
class UDRInventoryWidget;
class UDRInventoryScreenViewModel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRInventoryScreenEntryClicked, FGuid, InstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRInventoryScreenCloseRequested);

/** 로컬 플레이어를 중앙에 고정해 최대 3명의 인벤토리 패널을 표시한다. */
UCLASS()
class DEEPRAIDERS_API UDRInventoryScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeScreen(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Inventory|MVVM")
	void SetLeftPanel(UDRInventoryViewModel* PanelViewModel);

	UFUNCTION(BlueprintCallable, Category = "Inventory|MVVM")
	void SetCenterPanel(UDRInventoryViewModel* PanelViewModel);

	UFUNCTION(BlueprintCallable, Category = "Inventory|MVVM")
	void SetRightPanel(UDRInventoryViewModel* PanelViewModel);

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventoryScreenEntryClicked OnEntryClickedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDRInventoryScreenCloseRequested OnCloseRequestedDelegate;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleEntryClicked(FGuid InstanceId);

	UFUNCTION()
	void HandleCloseClicked();

	void InitializePanel(UDRInventoryWidget* Panel, UDRInventoryViewModel* PanelViewModel);

protected:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UDRInventoryWidget> LeftInventoryPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UDRInventoryWidget> CenterInventoryPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UDRInventoryWidget> RightInventoryPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

private:

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|MVVM")
	FName ScreenViewModelName = TEXT("DRInventoryScreenViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryScreenViewModel> ScreenViewModel;
};
