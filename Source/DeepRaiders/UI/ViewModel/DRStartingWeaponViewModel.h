#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DRStartingWeaponViewModel.generated.h"

class ADRPlayerController;
class UDataTable;
class UTexture2D;
class UDRStartingWeaponViewModel;

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

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRStartingWeaponViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(ADRPlayerController* InPlayerController, UDataTable* InWeaponTable);

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

	TWeakObjectPtr<ADRPlayerController> PlayerController;
};
