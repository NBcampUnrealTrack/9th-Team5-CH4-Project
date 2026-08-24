#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRStartingWeaponSelectWidget.generated.h"

class ADRPlayerController;
class UButton;
class UDataTable;
class UListView;
class UDRStartingWeaponViewModel;

UCLASS()
class DEEPRAIDERS_API UDRStartingWeaponSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDRStartingWeaponSelectWidget(const FObjectInitializer& ObjectInitializer);

	void InitializeSelection(
		ADRPlayerController* InPlayerController,
		UDataTable* InWeaponTable);
	void DeinitializeSelection();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Starting Weapon|Widget", meta = (BindWidget))
	TObjectPtr<UListView> WeaponListView;

	UPROPERTY(BlueprintReadOnly, Category = "Starting Weapon|Widget", meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

private:
	void HandleWeaponClicked(UObject* Item);

	UFUNCTION()
	void HandleConfirmClicked();

	UPROPERTY(EditDefaultsOnly, Category = "Starting Weapon|MVVM")
	FName ViewModelName = TEXT("DRStartingWeaponViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRStartingWeaponViewModel> ViewModel;
};
