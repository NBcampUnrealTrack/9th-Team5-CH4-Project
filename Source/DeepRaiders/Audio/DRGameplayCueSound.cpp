#include "DRGameplayCueSound.h"

#include "DRSoundLibrary.h"
#include "DRSoundSubsystem.h"
#include "Engine/World.h"

bool UDRGameplayCueSound::OnExecute_Implementation(AActor* Target,
	const FGameplayCueParameters& Parameters) const
{
	return RequestSound(Target, Parameters, false);
}

bool UDRGameplayCueSound::OnActive_Implementation(AActor* Target,
	const FGameplayCueParameters& Parameters) const
{
	return RequestSound(Target, Parameters, true);
}

bool UDRGameplayCueSound::WhileActive_Implementation(AActor* Target,
	const FGameplayCueParameters& Parameters) const
{
	return RequestSound(Target, Parameters, true);
}

bool UDRGameplayCueSound::OnRemove_Implementation(AActor* Target,
	const FGameplayCueParameters& Parameters) const
{
	if (!IsValid(Target) || !IsValid(SoundLibrary) || !Target->GetWorld())
	{
		return false;
	}

	const FGameplayTag SoundTag = ResolveSoundTag(Parameters);
	const FDRSoundDefinition* Definition = SoundLibrary->FindDefinition(SoundTag);
	if (!Definition || !Definition->bLoop)
	{
		return false;
	}

	AActor* SourceActor = ResolveSourceActor(Target, Parameters);
	if (!IsValid(SourceActor))
	{
		return false;
	}

	if (UDRSoundSubsystem* SoundSubsystem = Target->GetWorld()->GetSubsystem<UDRSoundSubsystem>())
	{
		SoundSubsystem->StopSound(SoundLibrary, SourceActor, SoundTag);
		return true;
	}

	return false;
}

float UDRGameplayCueSound::ResolvePriority_Implementation(AActor* Target,
	const FGameplayCueParameters& Parameters) const
{
	return -1.0f;
}

AActor* UDRGameplayCueSound::ResolveSourceActor_Implementation(AActor* Target,
	const FGameplayCueParameters& Parameters) const
{
	return Target;
}

FGameplayTag UDRGameplayCueSound::ResolveSoundTag(const FGameplayCueParameters& Parameters) const
{
	return Parameters.OriginalTag.IsValid() ? Parameters.OriginalTag : GameplayCueTag;
}

bool UDRGameplayCueSound::RequestSound(AActor* Target, const FGameplayCueParameters& Parameters,
	bool bLoopEvent) const
{
	if (!IsValid(Target) || !IsValid(SoundLibrary) || !Target->GetWorld())
	{
		return false;
	}

	const FGameplayTag SoundTag = ResolveSoundTag(Parameters);
	const FDRSoundDefinition* Definition = SoundLibrary->FindDefinition(SoundTag);
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SoundCue] Definition not found: %s"), *SoundTag.ToString());
		return false;
	}

	if (Definition->bLoop != bLoopEvent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SoundCue] Loop event mismatch: %s"), *SoundTag.ToString());
		return false;
	}

	UDRSoundSubsystem* SoundSubsystem = Target->GetWorld()->GetSubsystem<UDRSoundSubsystem>();
	if (!SoundSubsystem)
	{
		return false;
	}

	AActor* SourceActor = ResolveSourceActor(Target, Parameters);
	FDRSoundRequest Request;
	Request.SoundTag = SoundTag;
	Request.SourceActor = SourceActor;
	Request.Instigator = Parameters.GetInstigator();
	Request.WorldLocation = Parameters.Location;
	if (Request.WorldLocation.IsNearlyZero())
	{
		Request.WorldLocation = IsValid(SourceActor)
			? SourceActor->GetActorLocation()
			: Target->GetActorLocation();
	}
	Request.PriorityOverride = ResolvePriority(Target, Parameters);
	const bool bPlayed = SoundSubsystem->RequestSound(SoundLibrary, Request) != nullptr;
	if (!bPlayed)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SoundCue] Playback rejected: %s"), *SoundTag.ToString());
	}

	return bPlayed;
}
