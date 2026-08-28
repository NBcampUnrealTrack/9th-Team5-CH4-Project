#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HostOrJoinWidget.generated.h"

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
	UFUNCTION()
	void HandleSettingSliderChanged(float Value);

	void LoadSettingsIntoSliders();
	void RefreshSettingValueTexts();
	void ApplyAudioSettings(float MasterVolume, float MusicVolume, float SFXVolume);

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> RuntimeSoundMix;
};
