#include "DRGameUserSettings.h"

#include "Engine/Engine.h"

UDRGameUserSettings* UDRGameUserSettings::Get()
{
	return GEngine ? Cast<UDRGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UDRGameUserSettings::SetTitleSettings(
	float Master,
	float Music,
	float SFX,
	float MouseX,
	float MouseY)
{
	MasterVolume = FMath::Clamp(Master, 0.f, 1.f);
	MusicVolume = FMath::Clamp(Music, 0.f, 1.f);
	SFXVolume = FMath::Clamp(SFX, 0.f, 1.f);
	MouseSensitivityX = FMath::Clamp(MouseX, 0.001f, 5.f);
	MouseSensitivityY = FMath::Clamp(MouseY, 0.001f, 5.f);
}
