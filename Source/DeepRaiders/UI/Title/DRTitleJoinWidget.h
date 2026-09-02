#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRTitleJoinWidget.generated.h"

class UEditableTextBox;
class UOverlay;

UCLASS()
class DEEPRAIDERS_API UDRTitleJoinWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	void Show();

	UFUNCTION(BlueprintCallable, Category = "Title|Join")
	void HandleJoinClicked();

	UFUNCTION(BlueprintCallable, Category = "Title|Join")
	void HandleCloseJoinClicked();

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_Join;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ETB_IPAddress;
};
