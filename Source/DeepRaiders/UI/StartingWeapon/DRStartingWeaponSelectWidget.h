#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRStartingWeaponSelectWidget.generated.h"

class UButton;
class UListView;
class UDRStartingWeaponSelectionComponent;
class UDRStartingWeaponViewModel;

UCLASS()
class DEEPRAIDERS_API UDRStartingWeaponSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDRStartingWeaponSelectWidget(const FObjectInitializer& ObjectInitializer);

	/** 선택 컴포넌트와 MVVM ViewModel을 연결하고 목록을 구성한다. */
	void InitializeSelection(
		UDRStartingWeaponSelectionComponent* InSelectionComponent);

	/** 탭이 닫히거나 선택이 끝날 때 ViewModel 참조와 데이터를 정리한다. */
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
