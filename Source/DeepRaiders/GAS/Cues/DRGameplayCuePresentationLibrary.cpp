#include "DRGameplayCuePresentationLibrary.h"

#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"

void UDRGameplayCuePresentationLibrary::HandleLocalSoundCue(
	AActor* Target,
	const FGameplayTag& SoundCueTag,
	const FGameplayCueParameters& SourceParameters,
	EGameplayCueEvent::Type EventType)
{
	if (!IsValid(Target) || !SoundCueTag.IsValid())
	{
		return;
	}

	UGameplayCueManager* CueManager =
		UAbilitySystemGlobals::Get().GetGameplayCueManager();

	if (!IsValid(CueManager))
	{
		return;
	}

	FGameplayCueParameters Parameters = SourceParameters;

	// GC_Sound가 부모 태그여도 실제 요청한 child tag를 알 수 있게.
	Parameters.OriginalTag = SoundCueTag;

	CueManager->HandleGameplayCue(
		Target,
		SoundCueTag,
		EventType,
		Parameters);
}

void UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(
	AActor* Target,
	FGameplayTag SoundCueTag,
	const FGameplayCueParameters& SourceParameters)
{
	HandleLocalSoundCue(
		Target,
		SoundCueTag,
		SourceParameters,
		EGameplayCueEvent::Executed);
}

void UDRGameplayCuePresentationLibrary::ActivateLocalSoundCue(
	AActor* Target,
	FGameplayTag SoundCueTag,
	const FGameplayCueParameters& SourceParameters)
{
	HandleLocalSoundCue(
		Target,
		SoundCueTag,
		SourceParameters,
		EGameplayCueEvent::OnActive);
}

void UDRGameplayCuePresentationLibrary::RemoveLocalSoundCue(
	AActor* Target,
	FGameplayTag SoundCueTag,
	const FGameplayCueParameters& SourceParameters)
{
	HandleLocalSoundCue(
		Target,
		SoundCueTag,
		SourceParameters,
		EGameplayCueEvent::Removed);
}