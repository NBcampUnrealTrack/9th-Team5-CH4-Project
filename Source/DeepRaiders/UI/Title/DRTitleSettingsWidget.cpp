#include "DRTitleSettingsWidget.h"

#include "DRTitleComboBoxSettingRowWidget.h"
#include "DRTitleSettingRowWidget.h"
#include "DRTitleTextSettingRowWidget.h"
#include "Components/Overlay.h"
#include "DeepRaiders/Core/Settings/DRGameUserSettings.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"

namespace DRTitleSettings
{
	constexpr float MaxVolumeDisplay = 100.f;
	constexpr int32 ResolutionPercentages[] = { 100, 75, 50, 25 };

	const EWindowMode::Type ScreenModes[] =
	{
		EWindowMode::Fullscreen,
		EWindowMode::WindowedFullscreen,
		EWindowMode::Windowed
	};
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
	LoadPlayerNameIntoRow();
	LoadDisplaySettingsIntoRows();
	if (UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get())
	{
		UserSettings->ApplyAudioSettings(this);
	}

	return true;
}

void UDRTitleSettingsWidget::Show()
{
	LoadSettingsIntoSliders();
	LoadPlayerNameIntoRow();
	LoadDisplaySettingsIntoRows();
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

	if (IsValid(Settings_PlayerName))
	{
		UserSettings->SetPlayerDisplayName(Settings_PlayerName->GetSettingText().ToString());
		Settings_PlayerName->SetSettingText(
			FText::FromString(UserSettings->GetPlayerDisplayName()));
	}

	ApplyDisplaySettings();

	UserSettings->SaveSettings();
	UserSettings->ApplyAudioSettings(this);

	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestSetPlayerName(UserSettings->GetPlayerDisplayName());
	}

	Hide();
}

void UDRTitleSettingsWidget::HandleSettingsCancelClicked()
{
	LoadSettingsIntoSliders();
	LoadPlayerNameIntoRow();
	LoadDisplaySettingsIntoRows();
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

void UDRTitleSettingsWidget::LoadPlayerNameIntoRow()
{
	if (!IsValid(Settings_PlayerName))
	{
		return;
	}

	const UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();
	if (!IsValid(UserSettings))
	{
		return;
	}

	Settings_PlayerName->SetSettingText(
		FText::FromString(UserSettings->GetPlayerDisplayName()));
}

void UDRTitleSettingsWidget::LoadDisplaySettingsIntoRows()
{
	const UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();
	if (!IsValid(UserSettings))
	{
		return;
	}

	if (IsValid(Settings_ScreenMode))
	{
		Settings_ScreenMode->SetOptions({ TEXT("전체화면"), TEXT("Borderless"), TEXT("창모드") });
		Settings_ScreenMode->SetSelectedIndex(static_cast<int32>(UserSettings->GetFullscreenMode()));
	}

	if (!IsValid(Settings_Resolution))
	{
		return;
	}

	ResolutionOptions.Reset();
	TArray<FString> ResolutionLabels;
	const FIntPoint DesktopResolution = UserSettings->GetDesktopResolution();
	for (const int32 Percentage : DRTitleSettings::ResolutionPercentages)
	{
		const FIntPoint Resolution(
			FMath::RoundToInt(DesktopResolution.X * Percentage / 100.f),
			FMath::RoundToInt(DesktopResolution.Y * Percentage / 100.f));
		ResolutionOptions.Add(Resolution);
		ResolutionLabels.Add(FString::Printf(
			TEXT("%d x %d (%d%%)"), Resolution.X, Resolution.Y, Percentage));
	}

	Settings_Resolution->SetOptions(ResolutionLabels);
	int32 SelectedResolutionIndex = ResolutionOptions.IndexOfByKey(
		UserSettings->GetScreenResolution());
	if (SelectedResolutionIndex == INDEX_NONE)
	{
		SelectedResolutionIndex = 0;
	}
	Settings_Resolution->SetSelectedIndex(SelectedResolutionIndex);
}

void UDRTitleSettingsWidget::ApplyDisplaySettings()
{
	UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();
	if (!IsValid(UserSettings))
	{
		return;
	}

	bool bHasDisplaySetting = false;
	if (IsValid(Settings_ScreenMode))
	{
		const int32 ScreenModeIndex = Settings_ScreenMode->GetSelectedIndex();
		if (ScreenModeIndex >= 0 && ScreenModeIndex < UE_ARRAY_COUNT(DRTitleSettings::ScreenModes))
		{
			UserSettings->SetFullscreenMode(DRTitleSettings::ScreenModes[ScreenModeIndex]);
			bHasDisplaySetting = true;
		}
	}

	if (IsValid(Settings_Resolution))
	{
		const int32 ResolutionIndex = Settings_Resolution->GetSelectedIndex();
		if (ResolutionOptions.IsValidIndex(ResolutionIndex))
		{
			UserSettings->SetScreenResolution(ResolutionOptions[ResolutionIndex]);
			bHasDisplaySetting = true;
		}
	}

	if (bHasDisplaySetting)
	{
		// 화면 옵션이 연결된 경우에만 창 상태를 변경한다.
		UserSettings->ApplyResolutionSettings(false);
	}
}
