#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "DRStartingSkillEntryWidget.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRStartingSkillEntryWidget
	: public UUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Starting Skill|MVVM")
	FName ViewModelName = TEXT("DRStartingSkillEntryViewModel");
};
