#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRTitleSettingsWidget.generated.h"

class UDRTitleSettingRowWidget;
class UOverlay;

UCLASS()
class DEEPRAIDERS_API UDRTitleSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	void Show();

	UFUNCTION(BlueprintCallable, Category = "Title|Settings")
	void HandleSettingsApplyClicked();

	UFUNCTION(BlueprintCallable, Category = "Title|Settings")
	void HandleSettingsCancelClicked();

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_Settings;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_MasterVolume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_SFXVolume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_MusicVolume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_MouseSensitivityX;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_MouseSensitivityY;

private:
	void Hide();
	void LoadSettingsIntoSliders();
};
