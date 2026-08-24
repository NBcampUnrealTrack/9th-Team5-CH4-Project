#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HostOrJoinWidget.generated.h"

class UButton;
class UEditableTextBox;
class UOverlay;
class USlider;
class USoundClass;
class USoundMix;
class UTextBlock;
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

protected:
	// 타이틀
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PublicMatch;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PrivateCreate;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PrivateMatch;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Settings;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ExitGame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_Join;

	// Join Server
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ETB_IPAddress;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Join;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_CloseJoin;

	// Settings
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_Settings;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> Slider_MasterVolume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> Slider_MusicVolume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> Slider_SFXVolume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> Slider_MouseSensitivityX;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> Slider_MouseSensitivityY;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Apply;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Cancel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MasterVolumn;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MusicVolumn;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SFXVolumn;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MouseXAxis;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MouseYAxis;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session")
	FString DedicatedServerAddress = TEXT("127.0.0.1:7777");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session")
	TSoftObjectPtr<UWorld> PlayMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> SFXSoundClass;

private:
	UFUNCTION()
	void HandlePublicMatchClicked();

	UFUNCTION()
	void HandlePrivateCreateClicked();

	UFUNCTION()
	void HandlePrivateMatchClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleCloseJoinClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleSettingsApplyClicked();

	UFUNCTION()
	void HandleSettingsCancelClicked();

	UFUNCTION()
	void HandleSettingSliderChanged(float Value);

	UFUNCTION()
	void HandleExitGameClicked();

	UFUNCTION()
	void HandleJoinSessionComplete(bool bWasSuccessful);

	void LoadSettingsIntoSliders();
	void RefreshSettingValueTexts();
	void ApplyAudioSettings(float MasterVolume, float MusicVolume, float SFXVolume);

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> RuntimeSoundMix;
};
