#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRMenuWidget.generated.h"

class UDRTitleSettingsWidget;

/** 게임 중 Esc로 표시되는 메뉴를 제어한다. */
UCLASS()
class DEEPRAIDERS_API UDRMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void HandleSettingsClicked();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void HandleMoveToTitleClicked();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void HandleCloseClicked();

protected:
	/** WBP_Menu 내부에 설정 패널을 배치한 경우 재사용한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRTitleSettingsWidget> WBP_Settings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
	FName TitleMapName = TEXT("/Game/DeepRaiders/Maps/Development/TitleMap");
};
