#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HostOrJoinWidget.generated.h"

class UEditableTextBox;
class UButton;

UCLASS()
class DEEPRAIDERS_API UHostOrJoinWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Host;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Join;

	UPROPERTY(meta = (BindWidget, OptionalWidget))
	TObjectPtr<UEditableTextBox> ETB_IPAddress;

public:
	virtual bool Initialize() override;

private:
	UFUNCTION()
	void OnJoinButtonClicked();

	UFUNCTION()
	void OnJoinSessionComplete(bool bWasSuccessful);
};
