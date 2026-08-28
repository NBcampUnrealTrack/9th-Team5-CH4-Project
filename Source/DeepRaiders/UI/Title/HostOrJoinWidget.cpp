#include "HostOrJoinWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "DeepRaiders/Core/Settings/DRGameUserSettings.h"
#include "DeepRaiders/Core/Subsystem/DRSessionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace DRTitleSettings
{
	constexpr float MaxVolumeDisplay = 100.f;
	constexpr float MinMouseSensitivity = 0.001f;
	constexpr float MaxMouseSensitivity = 5.f;
	constexpr TCHAR MasterSoundClassPath[] =
		TEXT("/Game/DeepRaiders/Sound/SoundClass/SC_Master.SC_Master");
	constexpr TCHAR MusicSoundClassPath[] =
		TEXT("/Game/DeepRaiders/Sound/SoundClass/SC_Music.SC_Music");
	constexpr TCHAR SFXSoundClassPath[] =
		TEXT("/Game/DeepRaiders/Sound/SoundClass/SC_SFX.SC_SFX");
}

bool UHostOrJoinWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// 디자이너 프리뷰에는 GameInstance가 없으므로 런타임 바인딩을 생략한다.
	if (IsDesignTime())
	{
		return true;
	}

	// 타이틀
	PublicMatch->OnClicked.AddDynamic(this, &ThisClass::HandlePublicMatchClicked);
	PrivateCreate->OnClicked.AddDynamic(this, &ThisClass::HandlePrivateCreateClicked);
	ExitGame->OnClicked.AddDynamic(this, &ThisClass::HandleExitGameClicked);

	// Join Server
	PrivateMatch->OnClicked.AddDynamic(this, &ThisClass::HandlePrivateMatchClicked);
	Btn_Join->OnClicked.AddDynamic(this, &ThisClass::HandleJoinClicked);
	Btn_CloseJoin->OnClicked.AddDynamic(this, &ThisClass::HandleCloseJoinClicked);

	// Settings
	Settings->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsClicked);
	Button_Apply->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsApplyClicked);
	Button_Cancel->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsCancelClicked);
	Slider_MasterVolume->OnValueChanged.AddDynamic(this, &ThisClass::HandleSettingSliderChanged);
	Slider_MusicVolume->OnValueChanged.AddDynamic(this, &ThisClass::HandleSettingSliderChanged);
	Slider_SFXVolume->OnValueChanged.AddDynamic(this, &ThisClass::HandleSettingSliderChanged);
	Slider_MouseSensitivityX->OnValueChanged.AddDynamic(this, &ThisClass::HandleSettingSliderChanged);
	Slider_MouseSensitivityY->OnValueChanged.AddDynamic(this, &ThisClass::HandleSettingSliderChanged);

	Overlay_Join->SetVisibility(ESlateVisibility::Collapsed);
	Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);

	LoadSettingsIntoSliders();
	if (const UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get())
	{
		ApplyAudioSettings(
			UserSettings->GetMasterVolume(),
			UserSettings->GetMusicVolume(),
			UserSettings->GetSFXVolume());
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (IsValid(GameInstance))
	{
		if (UDRSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UDRSessionSubsystem>())
		{
			SessionSubsystem->OnJoinSessionComplete.AddDynamic(this, &ThisClass::HandleJoinSessionComplete);
		}
	}

	return true;
}

// 서버 매치
void UHostOrJoinWidget::JoinPrivateMatch(const FString& Address)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return;
	}

	if (UDRSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UDRSessionSubsystem>())
	{
		SessionSubsystem->JoinListenServer(Address);
	}
}

void UHostOrJoinWidget::HandlePublicMatchClicked()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (IsValid(GameInstance))
	{
		if (UDRSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UDRSessionSubsystem>())
		{
			SessionSubsystem->JoinDedicatedServer(DedicatedServerAddress);
			return;
		}
	}

	PublicMatch->SetIsEnabled(true);
}

void UHostOrJoinWidget::HandlePrivateCreateClicked()
{
	if (PlayMap.IsNull())
	{
		return;
	}

	PrivateCreate->SetIsEnabled(false);
	UGameInstance* GameInstance = GetGameInstance();
	if (!IsValid(GameInstance))
	{
		PrivateCreate->SetIsEnabled(true);
		return;
	}

	if (UDRSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UDRSessionSubsystem>())
	{
		SessionSubsystem->CreateListenServerSession(PlayMap);
		return;
	}

	PrivateCreate->SetIsEnabled(true);
}

void UHostOrJoinWidget::HandlePrivateMatchClicked()
{
	Overlay_Join->SetVisibility(ESlateVisibility::Visible);
	ETB_IPAddress->SetKeyboardFocus();
}

void UHostOrJoinWidget::HandleJoinClicked()
{
	const FString Address = ETB_IPAddress->GetText().ToString().TrimStartAndEnd();
	if (Address.IsEmpty())
	{
		return;
	}
	
	JoinPrivateMatch(Address);
}

void UHostOrJoinWidget::HandleCloseJoinClicked()
{
	Overlay_Join->SetVisibility(ESlateVisibility::Collapsed);
}

// 세팅
void UHostOrJoinWidget::HandleSettingsClicked()
{
	LoadSettingsIntoSliders();
	Overlay_Settings->SetVisibility(ESlateVisibility::Visible);
}

void UHostOrJoinWidget::HandleSettingsApplyClicked()
{
	UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();
	if (!IsValid(UserSettings))
	{
		return;
	}

	const float MouseX = FMath::Lerp(
		DRTitleSettings::MinMouseSensitivity,
		DRTitleSettings::MaxMouseSensitivity,
		Slider_MouseSensitivityX->GetValue());
	const float MouseY = FMath::Lerp(
		DRTitleSettings::MinMouseSensitivity,
		DRTitleSettings::MaxMouseSensitivity,
		Slider_MouseSensitivityY->GetValue());

	UserSettings->SetTitleSettings(
		Slider_MasterVolume->GetValue(),
		Slider_MusicVolume->GetValue(),
		Slider_SFXVolume->GetValue(),
		MouseX,
		MouseY);
	UserSettings->ApplySettings(false);
	UserSettings->SaveSettings();
	ApplyAudioSettings(
		UserSettings->GetMasterVolume(),
		UserSettings->GetMusicVolume(),
		UserSettings->GetSFXVolume());
	Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);
}

void UHostOrJoinWidget::HandleSettingsCancelClicked()
{
	LoadSettingsIntoSliders();
	Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);
}

void UHostOrJoinWidget::HandleSettingSliderChanged(float)
{
	RefreshSettingValueTexts();
}

void UHostOrJoinWidget::HandleExitGameClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UHostOrJoinWidget::HandleJoinSessionComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		PublicMatch->SetIsEnabled(true);
		Btn_Join->SetIsEnabled(true);
	}
}

void UHostOrJoinWidget::LoadSettingsIntoSliders()
{
	const UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();
	if (!IsValid(UserSettings))
	{
		return;
	}

	Slider_MasterVolume->SetValue(UserSettings->GetMasterVolume());
	Slider_MusicVolume->SetValue(UserSettings->GetMusicVolume());
	Slider_SFXVolume->SetValue(UserSettings->GetSFXVolume());
	const FVector2D SensitivityRange(
		DRTitleSettings::MinMouseSensitivity,
		DRTitleSettings::MaxMouseSensitivity);
	Slider_MouseSensitivityX->SetValue(FMath::GetMappedRangeValueClamped(
		SensitivityRange,
		FVector2D(0.f, 1.f),
		UserSettings->GetMouseSensitivityX()));
	Slider_MouseSensitivityY->SetValue(FMath::GetMappedRangeValueClamped(
		SensitivityRange,
		FVector2D(0.f, 1.f),
		UserSettings->GetMouseSensitivityY()));
	RefreshSettingValueTexts();
}

void UHostOrJoinWidget::RefreshSettingValueTexts()
{
	MasterVolumn->SetText(FText::AsNumber(FMath::RoundToInt(
		Slider_MasterVolume->GetValue() * DRTitleSettings::MaxVolumeDisplay)));
	MusicVolumn->SetText(FText::AsNumber(FMath::RoundToInt(
		Slider_MusicVolume->GetValue() * DRTitleSettings::MaxVolumeDisplay)));
	SFXVolumn->SetText(FText::AsNumber(FMath::RoundToInt(
		Slider_SFXVolume->GetValue() * DRTitleSettings::MaxVolumeDisplay)));
	FNumberFormattingOptions SensitivityFormat;
	SensitivityFormat.MinimumFractionalDigits = 3;
	SensitivityFormat.MaximumFractionalDigits = 3;
	const float MouseX = FMath::Lerp(
		DRTitleSettings::MinMouseSensitivity,
		DRTitleSettings::MaxMouseSensitivity,
		Slider_MouseSensitivityX->GetValue());
	const float MouseY = FMath::Lerp(
		DRTitleSettings::MinMouseSensitivity,
		DRTitleSettings::MaxMouseSensitivity,
		Slider_MouseSensitivityY->GetValue());
	MouseXAxis->SetText(FText::AsNumber(MouseX, &SensitivityFormat));
	MouseYAxis->SetText(FText::AsNumber(MouseY, &SensitivityFormat));
}

void UHostOrJoinWidget::ApplyAudioSettings(float MasterVolume, float MusicVolume, float SFXVolume)
{
	// BP에 SoundClass가 지정되지 않아도 프로젝트 기본 SoundClass를 사용한다.
	if (!IsValid(MasterSoundClass))
	{
		MasterSoundClass = LoadObject<USoundClass>(nullptr, DRTitleSettings::MasterSoundClassPath);
	}
	if (!IsValid(MusicSoundClass))
	{
		MusicSoundClass = LoadObject<USoundClass>(nullptr, DRTitleSettings::MusicSoundClassPath);
	}
	if (!IsValid(SFXSoundClass))
	{
		SFXSoundClass = LoadObject<USoundClass>(nullptr, DRTitleSettings::SFXSoundClassPath);
	}

	if (!IsValid(RuntimeSoundMix))
	{
		RuntimeSoundMix = NewObject<USoundMix>(this);
		UGameplayStatics::PushSoundMixModifier(this, RuntimeSoundMix);
	}

	if (IsValid(MasterSoundClass))
	{
		UGameplayStatics::SetSoundMixClassOverride(this, RuntimeSoundMix, MasterSoundClass, MasterVolume);
	}
	if (IsValid(MusicSoundClass))
	{
		UGameplayStatics::SetSoundMixClassOverride(this, RuntimeSoundMix, MusicSoundClass, MusicVolume);
	}
	if (IsValid(SFXSoundClass))
	{
		UGameplayStatics::SetSoundMixClassOverride(this, RuntimeSoundMix, SFXSoundClass, SFXVolume);
	}
}
