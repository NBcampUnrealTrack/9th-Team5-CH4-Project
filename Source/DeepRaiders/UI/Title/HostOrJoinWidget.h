#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "HostOrJoinWidget.generated.h"

class UEditableTextBox;
class UDataTable;
class UImage;
class UOverlay;
class UWidget;
class UDRTitleSettingRowWidget;
class USoundClass;
class USoundMix;
class UTexture2D;
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
	void HandleCreateMapClicked();

	UFUNCTION(BlueprintCallable, Category = "Title")
	void HandleCloseChoiceMapClicked();

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

	// 비공개 리슨 서버 맵 선택 UI
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_ChoiceMap;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> ComboBoxString_ChoiceMap;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CreateMap;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CloseChoiceMap;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ChoosedImageMap;

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
	TObjectPtr<UDataTable> MapDefinitionTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	TObjectPtr<USoundClass> SFXSoundClass;

private:
	UFUNCTION()
	void HandleMapSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void CacheSettingRows();
	void LoadSettingsIntoSliders();
	void ApplyAudioSettings(float MasterVolume, float MusicVolume, float SFXVolume);
	bool HasAllSettingRows() const;
	void RefreshMapOptions();
	void SelectMapDefinition(int32 DefinitionIndex);
	void SelectPlayMap(TSoftObjectPtr<UWorld> InPlayMap, UTexture2D* InPreview);

	TArray<FName> MapDefinitionRowNames;

	UPROPERTY(Transient)
	TSoftObjectPtr<UWorld> SelectedPlayMap;

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
