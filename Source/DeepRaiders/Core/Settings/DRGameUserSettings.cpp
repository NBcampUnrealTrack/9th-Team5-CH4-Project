#include "DRGameUserSettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace DRGamePlayerNameSettings
{
	constexpr int32 MaxPlayerNameLength = 16;
}

namespace DRGameAudioSettings
{
	constexpr TCHAR MasterSoundClassPath[] =
		TEXT("/Game/DeepRaiders/Sound/SoundClass/SC_Master.SC_Master");
	constexpr TCHAR MusicSoundClassPath[] =
		TEXT("/Game/DeepRaiders/Sound/SoundClass/SC_Music.SC_Music");
	constexpr TCHAR SFXSoundClassPath[] =
		TEXT("/Game/DeepRaiders/Sound/SoundClass/SC_SFX.SC_SFX");
}

UDRGameUserSettings* UDRGameUserSettings::Get()
{
	return GEngine ? Cast<UDRGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UDRGameUserSettings::SetTitleSettings(float Master, float Music, float SFX, float MouseX, float MouseY)
{
	MasterVolume = FMath::Clamp(Master, 0.f, 1.f);
	MusicVolume = FMath::Clamp(Music, 0.f, 1.f);
	SFXVolume = FMath::Clamp(SFX, 0.f, 1.f);
	MouseSensitivityX = FMath::Clamp(MouseX, 0.001f, 5.f);
	MouseSensitivityY = FMath::Clamp(MouseY, 0.001f, 5.f);
}

FString UDRGameUserSettings::SanitizePlayerDisplayName(const FString& InPlayerDisplayName)
{
	FString SanitizedName = InPlayerDisplayName.TrimStartAndEnd();
	SanitizedName.ReplaceInline(TEXT("\r"), TEXT(""));
	SanitizedName.ReplaceInline(TEXT("\n"), TEXT(""));
	SanitizedName.ReplaceInline(TEXT("\t"), TEXT(""));

	if (SanitizedName.Len() > DRGamePlayerNameSettings::MaxPlayerNameLength)
	{
		SanitizedName.LeftInline(DRGamePlayerNameSettings::MaxPlayerNameLength);
	}

	return SanitizedName.IsEmpty() ? TEXT("Player") : SanitizedName;
}

void UDRGameUserSettings::SetPlayerDisplayName(const FString& NewPlayerDisplayName)
{
	PlayerDisplayName = SanitizePlayerDisplayName(NewPlayerDisplayName);
}

void UDRGameUserSettings::ApplyAudioSettings(const UObject* WorldContextObject)
{
	USoundClass* MasterSoundClass = LoadObject<USoundClass>(
		nullptr,
		DRGameAudioSettings::MasterSoundClassPath);
	USoundClass* MusicSoundClass = LoadObject<USoundClass>(
		nullptr,
		DRGameAudioSettings::MusicSoundClassPath);
	USoundClass* SFXSoundClass = LoadObject<USoundClass>(
		nullptr,
		DRGameAudioSettings::SFXSoundClassPath);

	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	FAudioDevice* AudioDevice = IsValid(World) ? World->GetAudioDeviceRaw() : nullptr;
	if (!IsValid(MasterSoundClass) || !IsValid(MusicSoundClass) || !IsValid(SFXSoundClass)
		|| AudioDevice == nullptr)
	{
		return;
	}

	if (!IsValid(RuntimeSoundMix))
	{
		RuntimeSoundMix = NewObject<USoundMix>(this);
		RuntimeSoundMix->InitialDelay = 0.f;
		RuntimeSoundMix->FadeInTime = 0.f;
		RuntimeSoundMix->Duration = -1.f;
		RuntimeSoundMix->FadeOutTime = 0.f;
	}

	// 같은 믹스가 중첩되지 않도록 현재 월드에서 한 번 교체한다.
	AudioDevice->PopSoundMixModifier(RuntimeSoundMix);
	AudioDevice->SetSoundMixClassOverride(
		RuntimeSoundMix, MasterSoundClass, MasterVolume, 1.f, 0.f, true);
	AudioDevice->SetSoundMixClassOverride(
		RuntimeSoundMix, MusicSoundClass, MusicVolume, 1.f, 0.f, true);
	AudioDevice->SetSoundMixClassOverride(
		RuntimeSoundMix, SFXSoundClass, SFXVolume, 1.f, 0.f, true);
	AudioDevice->PushSoundMixModifier(RuntimeSoundMix);

	UE_LOG(LogTemp, Log, TEXT("[Settings] Audio applied Master=%.2f Music=%.2f SFX=%.2f"),
		MasterVolume, MusicVolume, SFXVolume);
}
