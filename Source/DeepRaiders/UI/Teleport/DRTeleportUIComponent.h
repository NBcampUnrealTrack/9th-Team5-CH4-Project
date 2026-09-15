#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRTeleportUIComponent.generated.h"

class ADRPlayerController;
class ADRTeleportPoint;
class UDRTeleportComponent;
class UDRTeleportSelectWidget;
class UDRUIManagerSubsystem;

UCLASS()
class DEEPRAIDERS_API UDRTeleportUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRTeleportUIComponent();
	void RefreshTeleportComponentBinding();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindTeleportComponent(UDRTeleportComponent* NewTeleportComponent);
	void CloseTeleportSelectWidget();
	void SetTeleportOpenTag(bool bIsOpen) const;

	UFUNCTION()
	void HandleTeleportUseRequested(ADRTeleportPoint* CurrentTeleportPoint);

	UFUNCTION()
	void HandleTeleportCloseRequested();

	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UDRUIManagerSubsystem> UIManager;

	UPROPERTY(Transient)
	TObjectPtr<UDRTeleportComponent> BoundTeleportComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRTeleportSelectWidget> TeleportSelectWidget;
};
