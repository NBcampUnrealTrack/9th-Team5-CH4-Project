#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRPerkResetTestWidget.generated.h"

class UButton;

UCLASS()
class DEEPRAIDERS_API UDRPerkResetTestWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleResetButtonClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ResetButton;
};
