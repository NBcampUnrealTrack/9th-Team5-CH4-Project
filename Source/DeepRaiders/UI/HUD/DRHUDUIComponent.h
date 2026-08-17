#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRHUDUIComponent.generated.h"

class UDRHUDViewModel;
class UUserWidget;

/** 로컬 플레이어의 HUD 위젯과 ViewModel 생명주기를 관리한다. */
UCLASS(ClassGroup = UI, meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRHUDUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRHUDUIComponent();

	/** 현재 컨트롤러가 소유한 캐릭터를 HUD ViewModel에 다시 연결한다. */
	void RefreshPlayerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|MVVM")
	FName HUDViewModelName = TEXT("DRHUDViewModel");

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRHUDViewModel> HUDViewModel;
};
