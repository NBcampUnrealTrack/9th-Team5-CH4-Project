#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRLoadingUIComponent.generated.h"

class UDRLoadingViewModel;
class UDRUIManagerSubsystem;
class UUserWidget;

/** Local PlayerController의 중도 난입 화면과 MVVM 데이터 생명주기를 관리한다. */
UCLASS()
class DEEPRAIDERS_API UDRLoadingUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRLoadingUIComponent();

	/** UIManager 설정 직후 및 수신 시작 시 폴링을 기다리지 않고 반영한다. */
	void RefreshLoadingScreen();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void CloseLoadingScreen();
	void HandleCloseRequested();

	UPROPERTY(Transient)
	TObjectPtr<UDRUIManagerSubsystem> UIManager;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LoadingWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDRLoadingViewModel> LoadingViewModel;

	bool bViewModelBound = false;
	bool bReportedSetupError = false;
};
