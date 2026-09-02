#include "HostOrJoinWidget.h"

#include "AudioDevice.h"
#include "DRTitleMapDefinition.h"
#include "DRTitleSettingRowWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "DeepRaiders/Core/Settings/DRGameUserSettings.h"
#include "DeepRaiders/Core/Subsystem/DRSessionSubsystem.h"
#include "Engine/Texture2D.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace DRTitleSettings
{
	constexpr float MaxVolumeDisplay = 100.f;
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

	Overlay_Join->SetVisibility(ESlateVisibility::Collapsed);
	Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);
	Overlay_ChoiceMap->SetVisibility(ESlateVisibility::Collapsed);

	ComboBoxString_ChoiceMap->OnSelectionChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleMapSelectionChanged);
	RefreshMapOptions();

	CacheSettingRows();
	LoadSettingsIntoSliders();
	if (const UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get())
	{
		ApplyAudioSettings(
			UserSettings->GetMasterVolume(),
			UserSettings->GetMusicVolume(),
			UserSettings->GetSFXVolume());
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

}

void UHostOrJoinWidget::HandlePrivateCreateClicked()
{
	Overlay_ChoiceMap->SetVisibility(ESlateVisibility::Visible);
	SelectMapDefinition(ComboBoxString_ChoiceMap->GetSelectedIndex());
}

void UHostOrJoinWidget::HandleMapSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectedItem;
	(void)SelectionType;

	SelectMapDefinition(ComboBoxString_ChoiceMap->GetSelectedIndex());
}

void UHostOrJoinWidget::HandleCreateMapClicked()
{
	if (SelectedPlayMap.IsNull())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return;
	}

	if (UDRSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UDRSessionSubsystem>())
	{
		SessionSubsystem->CreateListenServerSession(SelectedPlayMap);
	}
}

void UHostOrJoinWidget::HandleCloseChoiceMapClicked()
{
	Overlay_ChoiceMap->SetVisibility(ESlateVisibility::Collapsed);
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
	UE_LOG(LogTemp, Log, TEXT("Title settings apply clicked."));

	UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();
	if (!IsValid(UserSettings))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Title settings apply failed: DRGameUserSettings is not active."));
		return;
	}

	CacheSettingRows();
	if (!HasAllSettingRows())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Title settings apply failed: setting rows were not found."));
		return;
	}

	UserSettings->SetTitleSettings(
		MasterVolumeRow->GetSettingValue() / DRTitleSettings::MaxVolumeDisplay,
		MusicVolumeRow->GetSettingValue() / DRTitleSettings::MaxVolumeDisplay,
		SFXVolumeRow->GetSettingValue() / DRTitleSettings::MaxVolumeDisplay,
		MouseSensitivityXRow->GetSettingValue(),
		MouseSensitivityYRow->GetSettingValue());
	UserSettings->ApplySettings(false);
	UserSettings->SaveSettings();
	ApplyAudioSettings(
		UserSettings->GetMasterVolume(),
		UserSettings->GetMusicVolume(),
		UserSettings->GetSFXVolume());
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Title settings applied: Master=%.2f Music=%.2f SFX=%.2f ")
		TEXT("MouseX=%.3f MouseY=%.3f"),
		UserSettings->GetMasterVolume(),
		UserSettings->GetMusicVolume(),
		UserSettings->GetSFXVolume(),
		UserSettings->GetMouseSensitivityX(),
		UserSettings->GetMouseSensitivityY());
	Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);
}

void UHostOrJoinWidget::HandleSettingsCancelClicked()
{
	LoadSettingsIntoSliders();
	Overlay_Settings->SetVisibility(ESlateVisibility::Collapsed);
}

void UHostOrJoinWidget::RefreshMapOptions()
{
	ComboBoxString_ChoiceMap->ClearOptions();
	MapDefinitionRowNames.Reset();

	if (!IsValid(MapDefinitionTable))
	{
		CreateMap->SetIsEnabled(false);
		return;
	}

	TArray<FName> DefinitionRowNames = MapDefinitionTable->GetRowNames();
	DefinitionRowNames.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName RowName : DefinitionRowNames)
	{
		const FDRTitleMapDefinition* Definition = MapDefinitionTable->FindRow<FDRTitleMapDefinition>(
			RowName,
			TEXT("Populate title map options"));
		if (Definition == nullptr)
		{
			continue;
		}

		const FString OptionName = Definition->DisplayName.IsEmpty()
			? RowName.ToString()
			: Definition->DisplayName.ToString();
		MapDefinitionRowNames.Add(RowName);
		ComboBoxString_ChoiceMap->AddOption(OptionName);
	}

	if (ComboBoxString_ChoiceMap->GetOptionCount() > 0)
	{
		ComboBoxString_ChoiceMap->SetSelectedIndex(0);
		SelectMapDefinition(0);
		return;
	}

	CreateMap->SetIsEnabled(false);
}

void UHostOrJoinWidget::SelectMapDefinition(int32 DefinitionIndex)
{
	if (!MapDefinitionRowNames.IsValidIndex(DefinitionIndex) || !IsValid(MapDefinitionTable))
	{
		SelectPlayMap(TSoftObjectPtr<UWorld>(), nullptr);
		return;
	}

	const FDRTitleMapDefinition* Definition = MapDefinitionTable->FindRow<FDRTitleMapDefinition>(
		MapDefinitionRowNames[DefinitionIndex],
		TEXT("Select title map"));
	if (Definition == nullptr)
	{
		SelectPlayMap(TSoftObjectPtr<UWorld>(), nullptr);
		return;
	}

	SelectPlayMap(Definition->Map, Definition->PreviewImage.LoadSynchronous());
}

void UHostOrJoinWidget::SelectPlayMap(TSoftObjectPtr<UWorld> InPlayMap, UTexture2D* InPreview)
{
	SelectedPlayMap = InPlayMap;
	CreateMap->SetIsEnabled(!SelectedPlayMap.IsNull());
	ChoosedImageMap->SetBrushFromTexture(InPreview);
}

void UHostOrJoinWidget::HandleExitGameClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UHostOrJoinWidget::CacheSettingRows()
{
	if (!IsValid(WidgetTree))
	{
		return;
	}

	MasterVolumeRow = nullptr;
	MusicVolumeRow = nullptr;
	SFXVolumeRow = nullptr;
	MouseSensitivityXRow = nullptr;
	MouseSensitivityYRow = nullptr;

	MasterVolumeRow = Cast<UDRTitleSettingRowWidget>(
		GetWidgetFromName(TEXT("Settings_MasterVolume")));
	SFXVolumeRow = Cast<UDRTitleSettingRowWidget>(
		GetWidgetFromName(TEXT("Settings_SFXVolume")));
	MusicVolumeRow = Cast<UDRTitleSettingRowWidget>(
		GetWidgetFromName(TEXT("Settings_MusicVolume")));
	MouseSensitivityXRow = Cast<UDRTitleSettingRowWidget>(
		GetWidgetFromName(TEXT("Settings_MouseSensitivityX")));
	MouseSensitivityYRow = Cast<UDRTitleSettingRowWidget>(
		GetWidgetFromName(TEXT("Settings_MouseSensitivityY")));
	if (HasAllSettingRows())
	{
		return;
	}

	// WBP_Title의 현재 세로 배치 순서로 직접 연결한다.
	if (IsValid(Settings_MasterVolume)
		&& IsValid(Settings_SFXVolume)
		&& IsValid(Settings_MusicVolume)
		&& IsValid(Settings_MouseSensitivityX)
		&& IsValid(Settings_MouseSensitivityY))
	{
		MasterVolumeRow = Settings_MasterVolume;
		SFXVolumeRow = Settings_SFXVolume;
		MusicVolumeRow = Settings_MusicVolume;
		MouseSensitivityXRow = Settings_MouseSensitivityX;
		MouseSensitivityYRow = Settings_MouseSensitivityY;
		return;
	}

	TArray<UDRTitleSettingRowWidget*> SettingRows;
	WidgetTree->ForEachWidgetAndDescendants([&SettingRows, this](UWidget* Widget)
	{
		UDRTitleSettingRowWidget* Row = Cast<UDRTitleSettingRowWidget>(Widget);
		if (!IsValid(Row))
		{
			return;
		}
		SettingRows.Add(Row);

		switch (Row->GetSettingType())
		{
		case EDRTitleSettingType::MasterVolume:
			MasterVolumeRow = Row;
			break;
		case EDRTitleSettingType::MusicVolume:
			MusicVolumeRow = Row;
			break;
		case EDRTitleSettingType::SFXVolume:
			SFXVolumeRow = Row;
			break;
		case EDRTitleSettingType::MouseSensitivityX:
			MouseSensitivityXRow = Row;
			break;
		case EDRTitleSettingType::MouseSensitivityY:
			MouseSensitivityYRow = Row;
			break;
		default:
			break;
		}
	});

	// 타입 설정이 빠진 경우 디자이너의 세로 배치 순서로 연결한다.
	if (!HasAllSettingRows() && SettingRows.Num() >= 5)
	{
		MasterVolumeRow = SettingRows[0];
		SFXVolumeRow = SettingRows[1];
		MusicVolumeRow = SettingRows[2];
		MouseSensitivityXRow = SettingRows[3];
		MouseSensitivityYRow = SettingRows[4];
	}
}

void UHostOrJoinWidget::LoadSettingsIntoSliders()
{
	const UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();
	if (!IsValid(UserSettings) || !HasAllSettingRows())
	{
		return;
	}

	MasterVolumeRow->SetSettingValue(
		UserSettings->GetMasterVolume() * DRTitleSettings::MaxVolumeDisplay);
	MusicVolumeRow->SetSettingValue(
		UserSettings->GetMusicVolume() * DRTitleSettings::MaxVolumeDisplay);
	SFXVolumeRow->SetSettingValue(UserSettings->GetSFXVolume() * DRTitleSettings::MaxVolumeDisplay);
	MouseSensitivityXRow->SetSettingValue(UserSettings->GetMouseSensitivityX());
	MouseSensitivityYRow->SetSettingValue(UserSettings->GetMouseSensitivityY());
}

bool UHostOrJoinWidget::HasAllSettingRows() const
{
	return IsValid(MasterVolumeRow)
		&& IsValid(MusicVolumeRow)
		&& IsValid(SFXVolumeRow)
		&& IsValid(MouseSensitivityXRow)
		&& IsValid(MouseSensitivityYRow);
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

	const bool bNeedsSoundMixPush = !IsValid(RuntimeSoundMix);
	if (bNeedsSoundMixPush)
	{
		RuntimeSoundMix = NewObject<USoundMix>(this);
		RuntimeSoundMix->InitialDelay = 0.f;
		RuntimeSoundMix->FadeInTime = 0.f;
		RuntimeSoundMix->Duration = -1.f;
		RuntimeSoundMix->FadeOutTime = 0.f;
	}

	FAudioDevice* AudioDevice = GetWorld() ? GetWorld()->GetAudioDeviceRaw() : nullptr;
	if (!AudioDevice)
	{
		return;
	}

	if (IsValid(MasterSoundClass))
	{
		AudioDevice->SetSoundMixClassOverride(
			RuntimeSoundMix, MasterSoundClass, MasterVolume, 1.f, 0.f, true);
	}
	if (IsValid(MusicSoundClass))
	{
		AudioDevice->SetSoundMixClassOverride(
			RuntimeSoundMix, MusicSoundClass, MusicVolume, 1.f, 0.f, true);
	}
	if (IsValid(SFXSoundClass))
	{
		AudioDevice->SetSoundMixClassOverride(
			RuntimeSoundMix, SFXSoundClass, SFXVolume, 1.f, 0.f, true);
	}
	if (bNeedsSoundMixPush)
	{
		AudioDevice->PushSoundMixModifier(RuntimeSoundMix);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Title audio settings applied to world audio device."));
}
