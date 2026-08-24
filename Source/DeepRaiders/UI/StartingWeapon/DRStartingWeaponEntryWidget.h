#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "DRStartingWeaponEntryWidget.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRStartingWeaponEntryWidget
	: public UUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Starting Weapon|MVVM")
	FName ViewModelName = TEXT("DRStartingWeaponEntryViewModel");
};
