#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HostOrJoinWidget.generated.h"

class UEditableTextBox;
class UOverlay;
class UDRTitleSettingRowWidget;
class USoundClass;
class USoundMix;
class UWorld;

UCLASS()
class DEEPRAIDERS_API UHostOrJoinWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	// Private 접속 팝업에서 입력한 호스트 주소로 이동한다.
	UFUNCTION(BlueprintCallable, Category = "Title")
	void JoinPrivateMatch(const FString& Address);

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
	TObjectPtr<UOverlay> Overlay_Join;

	// Join Server
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ETB_IPAddress;

	// Settings
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_Settings;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_MasterVolume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_SFXVolume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_MusicVolume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_MouseSensitivityX;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDRTitleSettingRowWidget> Settings_MouseSensitivityY;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session")
	FString DedicatedServerAddress = TEXT("shees95.myddns.me:17777");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session")
	TSoftObjectPtr<UWorld> PlayMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> SFXSoundClass;

private:
	void CacheSettingRows();
	void LoadSettingsIntoSliders();
	void ApplyAudioSettings(float MasterVolume, float MusicVolume, float SFXVolume);
	bool HasAllSettingRows() const;

	UPROPERTY(Transient)
	TObjectPtr<UDRTitleSettingRowWidget> MasterVolumeRow;

	UPROPERTY(Transient)
	TObjectPtr<UDRTitleSettingRowWidget> MusicVolumeRow;

	UPROPERTY(Transient)
	TObjectPtr<UDRTitleSettingRowWidget> SFXVolumeRow;

	UPROPERTY(Transient)
	TObjectPtr<UDRTitleSettingRowWidget> MouseSensitivityXRow;

	UPROPERTY(Transient)
	TObjectPtr<UDRTitleSettingRowWidget> MouseSensitivityYRow;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> RuntimeSoundMix;

};
