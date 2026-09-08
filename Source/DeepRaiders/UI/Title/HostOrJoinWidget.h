#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HostOrJoinWidget.generated.h"

class UDRTitleJoinWidget;
class UDRTitleMapChoiceWidget;
class UDRTitleSettingsWidget;

UCLASS()
class DEEPRAIDERS_API UHostOrJoinWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandlePublicMatchClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandlePrivateCreateClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandlePrivateMatchClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleSettingsClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleExitGameClicked();

	// 기존 WBP 이벤트가 남아 있어도 분리된 패널로 전달한다.
	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleCreateMapClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleCloseChoiceMapClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleJoinClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleCloseJoinClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleSettingsApplyClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleSettingsCancelClicked();

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleJoinWidget> WBP_Join;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleSettingsWidget> WBP_Settings;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleMapChoiceWidget> WBP_ChoiceMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session")
	FString DedicatedServerAddress = TEXT("shees95.myddns.me:17777");
	//FString DedicatedServerAddress = TEXT("katherine-fc.tun.ply.gg:55341");
};
