#include "DRTitleSettingsWidget.h"

#include "DRTitleSettingRowWidget.h"
#include "Components/Overlay.h"
#include "DeepRaiders/Core/Settings/DRGameUserSettings.h"

namespace DRTitleSettings
{
	constexpr float MaxVolumeDisplay = 100.f;
}

bool UDRTitleSettingsWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (IsDesignTime())
	{
		return true;
	}

	Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);
	LoadSettingsIntoSliders();
	if (UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get())
	{
		UserSettings->ApplyAudioSettings(this);
	}

	return true;
}

void UDRTitleSettingsWidget::Show()
{
	LoadSettingsIntoSliders();
	Overlay_Settings->SetVisibility(ESlateVisibility::Visible);
}

void UDRTitleSettingsWidget::HandleSettingsApplyClicked()
{
	UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();
	if (!IsValid(UserSettings))
	{
		return;
	}

	UserSettings->SetTitleSettings(
		Settings_MasterVolume->GetSettingValue() / DRTitleSettings::MaxVolumeDisplay,
		Settings_MusicVolume->GetSettingValue() / DRTitleSettings::MaxVolumeDisplay,
		Settings_SFXVolume->GetSettingValue() / DRTitleSettings::MaxVolumeDisplay,
		Settings_MouseSensitivityX->GetSettingValue(),
		Settings_MouseSensitivityY->GetSettingValue());
	UserSettings->ApplySettings(false);
	UserSettings->SaveSettings();
	UserSettings->ApplyAudioSettings(this);
	Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);
}

void UDRTitleSettingsWidget::HandleSettingsCancelClicked()
{
	LoadSettingsIntoSliders();
	Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);
}

void UDRTitleSettingsWidget::LoadSettingsIntoSliders()
{
	const UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();
	if (!IsValid(UserSettings))
	{
		return;
	}

	Settings_MasterVolume->SetSettingValue(
		UserSettings->GetMasterVolume() * DRTitleSettings::MaxVolumeDisplay);
	Settings_MusicVolume->SetSettingValue(
		UserSettings->GetMusicVolume() * DRTitleSettings::MaxVolumeDisplay);
	Settings_SFXVolume->SetSettingValue(
		UserSettings->GetSFXVolume() * DRTitleSettings::MaxVolumeDisplay);
	Settings_MouseSensitivityX->SetSettingValue(UserSettings->GetMouseSensitivityX());
	Settings_MouseSensitivityY->SetSettingValue(UserSettings->GetMouseSensitivityY());
}
