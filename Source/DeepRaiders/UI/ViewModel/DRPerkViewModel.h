#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DRPerkViewModel.generated.h"

class UDRPerkComponent;
class UDRPerkDefinition;
class UTexture2D;

/** 퍽 UI의 슬롯 한 칸에 표시할 상태다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRPerkEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Perk")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Perk")
	TObjectPtr<UTexture2D> Icon;

private:
	friend class UDRPerkViewModel;

	void Initialize(UDRPerkDefinition* NewPerkDefinition);
};

/** PerkComponent의 상태를 퍽 슬롯 목록으로 변환한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRPerkViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(UDRPerkComponent* NewPerkComponent);
	void Deinitialize();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Perk")
	TArray<TObjectPtr<UDRPerkEntryViewModel>> PerkEntries;

private:
	UFUNCTION()
	void RebuildPerkEntries();

	TWeakObjectPtr<UDRPerkComponent> PerkComponent;
};
