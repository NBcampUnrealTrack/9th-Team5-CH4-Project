#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRSkillUIComponent.generated.h"

class UDRSkillViewModel;
class UUserWidget;

/** HUD에 포함된 스킬 슬롯 위젯과 ViewModel 생명주기를 관리한다. */
UCLASS(ClassGroup = UI, meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRSkillUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRSkillUIComponent();

	void Initialize(UUserWidget* InHUDWidget);
	void RefreshPlayerCharacter();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool RegisterViewModel(UUserWidget* HUDWidget);

	UPROPERTY(Transient)
	TObjectPtr<UDRSkillViewModel> SkillViewModel;
};
