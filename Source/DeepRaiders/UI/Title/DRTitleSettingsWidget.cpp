#include "DRTitleSettingsWidget.h"

#include "DRTitleSettingRowWidget.h"
#include "Components/Overlay.h"
#include "DeepRaiders/Core/Settings/DRGameUserSettings.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"

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
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (auto* Manager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>())
		{
			// 열린 동안만 부모 화면보다 먼저 닫히는 팝업으로 등록한다.
			Manager->RegisterCloseHandler(
				this, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSettingsCancelClicked));
		}
	}
}

void UDRTitleSettingsWidget::Hide()
{
	if (IsValid(Overlay_Settings))
	{
		Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (auto* Manager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>())
		{
			Manager->UnregisterCloseHandler(this);
		}
	}
}

void UDRTitleSettingsWidget::NativeDestruct()
{
	Hide();
	Super::NativeDestruct();
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
	Hide();
}

void UDRTitleSettingsWidget::HandleSettingsCancelClicked()
{
	LoadSettingsIntoSliders();
	Hide();
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
