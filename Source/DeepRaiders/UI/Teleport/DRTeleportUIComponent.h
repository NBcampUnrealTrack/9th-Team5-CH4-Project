#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRTeleportUIComponent.generated.h"

class ADRPlayerController;
class ADRTeleportPoint;
class UDRTeleportComponent;
class UDRTeleportSelectWidget;

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

	UPROPERTY(EditDefaultsOnly, Category = "Teleport|UI")
	TSubclassOf<UDRTeleportSelectWidget> TeleportSelectWidgetClass;

private:
	void BindTeleportComponent(UDRTeleportComponent* NewTeleportComponent);
	void CloseTeleportSelectWidget();

	UFUNCTION()
	void HandleTeleportUseRequested(ADRTeleportPoint* CurrentTeleportPoint);

	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UDRTeleportComponent> BoundTeleportComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDRTeleportSelectWidget> TeleportSelectWidget;
};
