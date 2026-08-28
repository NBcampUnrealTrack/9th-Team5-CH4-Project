#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DRSkillViewModel.generated.h"

class ADRPlayerCharacter;
class UDRSkillSlotViewModel;

/** HUD 스킬 슬롯 두 개의 ViewModel을 관리한다. */
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRSkillViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(ADRPlayerCharacter* InPlayerCharacter);
	void Deinitialize();

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	TObjectPtr<UDRSkillSlotViewModel> SkillOne;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill")
	TObjectPtr<UDRSkillSlotViewModel> SkillTwo;
};
