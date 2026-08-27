#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DRStartingSkillViewModel.generated.h"

class UDRSkillDefinition;
class UDRStartingSkillViewModel;
class UDRStartingSelectionComponent;
class UTexture2D;

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRStartingSkillEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Select();

	const FText& GetDisplayName() const { return DisplayName; }
	const FText& GetDescription() const { return Description; }
	UTexture2D* GetIcon() const { return Icon; }

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Skill")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Skill")
	FText Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Skill")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Skill")
	bool IsSelected = false;

private:
	friend class UDRStartingSkillViewModel;

	void Initialize(
		UDRStartingSkillViewModel* InOwnerViewModel,
		FName InRowName,
		UDRSkillDefinition* InSkillDefinition);
	void SetSelected(bool IsNewSelected);

	TWeakObjectPtr<UDRStartingSkillViewModel> OwnerViewModel;
	FName RowName = NAME_None;
};

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRStartingSkillViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(UDRStartingSelectionComponent* InSelectionComponent);
	TArray<UDRStartingSkillEntryViewModel*> GetSkillEntries() const;

	UFUNCTION(BlueprintCallable, Category = "Starting Skill")
	void ConfirmSelection();

	void Deinitialize();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Skill")
	TArray<TObjectPtr<UDRStartingSkillEntryViewModel>> SkillEntries;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Starting Skill")
	bool IsConfirmEnabled = false;

private:
	friend class UDRStartingSkillEntryViewModel;

	void SelectSkill(UDRStartingSkillEntryViewModel* SkillEntry);

	UPROPERTY(Transient)
	TObjectPtr<UDRStartingSkillEntryViewModel> SelectedSkill;

	TWeakObjectPtr<UDRStartingSelectionComponent> SelectionComponent;
};
