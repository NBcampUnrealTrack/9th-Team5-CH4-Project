#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRShopWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRShopWidgetClosedSignature);

UCLASS()
class DEEPRAIDERS_API UDRShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Shop|UI")
	FDRShopWidgetClosedSignature OnCloseRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleCloseButtonClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;
};
