#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DRStartingWeaponViewModel.generated.h"

class UTexture2D;
class UDRStartingWeaponSelectionComponent;
class UDRStartingWeaponViewModel;

/** ListView 한 항목에 표시할 무기 정보와 선택 상태다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRStartingWeaponEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Starting Weapon")
	void Select();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Weapon")
	FName RowName = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Weapon")
	TObjectPtr<UDRProjectileWeaponItemDefinition> WeaponDefinition;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Weapon")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Weapon")
	FText Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Weapon")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Weapon")
	bool IsSelected = false;

private:
	friend class UDRStartingWeaponViewModel;

	void Initialize(
		UDRStartingWeaponViewModel* InOwnerViewModel,
		FName InRowName,
		UDRProjectileWeaponItemDefinition* InWeaponDefinition,
		const FText& InDescription);
	void SetSelected(bool IsNewSelected);

	TWeakObjectPtr<UDRStartingWeaponViewModel> OwnerViewModel;
};

/** DT의 무기 목록과 현재 선택 항목을 UI에 제공한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRStartingWeaponViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** 선택 컴포넌트의 DT를 읽어 ListView 항목을 생성한다. */
	void Initialize(UDRStartingWeaponSelectionComponent* InSelectionComponent);

	UFUNCTION(BlueprintCallable, Category = "Starting Weapon")
	void Deinitialize();

	UFUNCTION(BlueprintCallable, Category = "Starting Weapon")
	void ConfirmSelection();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Weapon")
	TArray<TObjectPtr<UDRStartingWeaponEntryViewModel>> WeaponEntries;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Weapon")
	TObjectPtr<UDRStartingWeaponEntryViewModel> SelectedWeapon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Weapon")
	bool IsConfirmEnabled = false;

private:
	friend class UDRStartingWeaponEntryViewModel;

	void SelectWeapon(UDRStartingWeaponEntryViewModel* WeaponEntry);

	TWeakObjectPtr<UDRStartingWeaponSelectionComponent> SelectionComponent;
};
