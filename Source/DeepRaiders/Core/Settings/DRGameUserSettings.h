#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "DRGameUserSettings.generated.h"

class USoundMix;

UCLASS(Config = GameUserSettings)
class DEEPRAIDERS_API UDRGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static UDRGameUserSettings* Get();

	void SetTitleSettings(float Master, float Music, float SFX, float MouseX, float MouseY);
	void ApplyAudioSettings(const UObject* WorldContextObject);

	float GetMasterVolume() const { return MasterVolume; }
	float GetMusicVolume() const { return MusicVolume; }
	float GetSFXVolume() const { return SFXVolume; }
	float GetMouseSensitivityX() const { return MouseSensitivityX; }
	float GetMouseSensitivityY() const { return MouseSensitivityY; }

private:
	UPROPERTY(Config)
	float MasterVolume = 1.f;

	UPROPERTY(Config)
	float MusicVolume = 1.f;

	UPROPERTY(Config)
	float SFXVolume = 1.f;

	UPROPERTY(Config)
	float MouseSensitivityX = 1.f;

	UPROPERTY(Config)
	float MouseSensitivityY = 1.f;

	/** 맵과 설정 위젯이 바뀌어도 하나만 유지하는 음량 믹스다. */
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> RuntimeSoundMix;
};
