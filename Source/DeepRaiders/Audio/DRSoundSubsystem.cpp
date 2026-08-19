#include "DRSoundSubsystem.h"

#include "DRSoundLibrary.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UAudioComponent* UDRSoundSubsystem::RequestSound(UDRSoundLibrary* Library, const FDRSoundRequest& Request)
{
	HandleAudioFinished(nullptr);

	if (!IsValid(Library) || !Request.SoundTag.IsValid())
	{
		return nullptr;
	}

	const FDRSoundDefinition* Definition = Library->FindDefinition(Request.SoundTag);
	if (!Definition || !IsValid(Definition->Sound))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SoundSubsystem] Sound is not assigned: %s"),
			*Request.SoundTag.ToString());
		return nullptr;
	}

	if (!ShouldPlayForLocalClient(*Definition, Request.Instigator))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SoundSubsystem] Audience rejected: %s"),
			*Request.SoundTag.ToString());
		return nullptr;
	}

	const FLoopKey LoopKey{Request.SourceActor, Request.SoundTag};
	if (Definition->bLoop)
	{
		if (!IsValid(Request.SourceActor))
		{
			return nullptr;
		}

		if (const TWeakObjectPtr<UAudioComponent>* ExistingComponent = ActiveLoops.Find(LoopKey))
		{
			if (ExistingComponent->IsValid())
			{
				return ExistingComponent->Get();
			}

			ActiveLoops.Remove(LoopKey);
		}
	}

	const float Priority = Request.PriorityOverride >= 0.0f
		? Request.PriorityOverride
		: Definition->DefaultPriority;
	UAudioComponent* AudioComponent = CreateAudioComponent(*Definition, Request, Priority);
	if (!AudioComponent)
	{
		return nullptr;
	}

	if (Definition->bLoop)
	{
		ActiveLoops.Add(LoopKey, AudioComponent);
		AudioComponent->OnAudioFinishedNative.AddUObject(this, &ThisClass::HandleAudioFinished);
	}

	AudioComponent->Play();
	return AudioComponent;
}

void UDRSoundSubsystem::StopSound(UDRSoundLibrary* Library, AActor* SourceActor,
	FGameplayTag SoundTag)
{
	if (!IsValid(Library) || !IsValid(SourceActor) || !SoundTag.IsValid())
	{
		return;
	}

	const FLoopKey LoopKey{SourceActor, SoundTag};
	TWeakObjectPtr<UAudioComponent> AudioComponent;
	if (!ActiveLoops.RemoveAndCopyValue(LoopKey, AudioComponent) || !AudioComponent.IsValid())
	{
		return;
	}

	const FDRSoundDefinition* Definition = Library->FindDefinition(SoundTag);
	const float FadeOutTime = Definition ? Definition->FadeOutTime : 0.0f;
	if (FadeOutTime > 0.0f)
	{
		AudioComponent->FadeOut(FadeOutTime, 0.0f);
		return;
	}

	AudioComponent->Stop();
}

UAudioComponent* UDRSoundSubsystem::CreateAudioComponent(const FDRSoundDefinition& Definition,
	const FDRSoundRequest& Request, float Priority) const
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	UObject* Outer = IsValid(Request.SourceActor) ? static_cast<UObject*>(Request.SourceActor) : World;
	UAudioComponent* AudioComponent = NewObject<UAudioComponent>(Outer);
	if (!AudioComponent)
	{
		return nullptr;
	}

	AudioComponent->bAutoDestroy = true;
	AudioComponent->bAutoActivate = false;
	AudioComponent->bStopWhenOwnerDestroyed = true;
	AudioComponent->bOverridePriority = true;
	AudioComponent->Priority = Definition.bAlwaysPlay ? MAX_flt : Priority;
	AudioComponent->VolumeMultiplier = Definition.VolumeMultiplier;
	AudioComponent->PitchMultiplier = Definition.PitchMultiplier;
	AudioComponent->SoundClassOverride = Definition.SoundClass;
	AudioComponent->SetSound(Definition.Sound);
	AudioComponent->SetAttenuationSettings(Definition.Attenuation);
	if (Definition.Concurrency && !Definition.bAlwaysPlay)
	{
		AudioComponent->ConcurrencySet.Add(Definition.Concurrency);
	}

	const bool bAttached = Definition.PlaybackMode == EDRSoundPlaybackMode::AttachedToSource_3D
		&& IsValid(Request.SourceActor) && IsValid(Request.SourceActor->GetRootComponent());
	AudioComponent->bAllowSpatialization = bAttached;
	AudioComponent->bIsUISound = !bAttached;
	AudioComponent->RegisterComponentWithWorld(World);

	if (bAttached)
	{
		AudioComponent->AttachToComponent(Request.SourceActor->GetRootComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	else if (Definition.PlaybackMode == EDRSoundPlaybackMode::AttachedToSource_3D)
	{
		AudioComponent->SetWorldLocation(Request.WorldLocation);
		AudioComponent->bAllowSpatialization = true;
		AudioComponent->bIsUISound = false;
	}

	return AudioComponent;
}

bool UDRSoundSubsystem::ShouldPlayForLocalClient(const FDRSoundDefinition& Definition, AActor* Instigator) const
{
	if (Definition.Audience == EDRSoundAudience::CueRecipients)
	{
		return true;
	}

	const AActor* Candidate = Instigator;
	for (int32 Depth = 0; Candidate && Depth < 4; ++Depth)
	{
		if (const APawn* Pawn = Cast<APawn>(Candidate))
		{
			return Pawn->IsLocallyControlled();
		}

		if (const AController* Controller = Cast<AController>(Candidate))
		{
			return Controller->IsLocalController();
		}

		Candidate = Candidate->GetOwner();
	}

	return false;
}

void UDRSoundSubsystem::HandleAudioFinished(UAudioComponent* AudioComponent)
{
	for (auto Iterator = ActiveLoops.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Value().IsValid() || Iterator.Value().Get() == AudioComponent)
		{
			Iterator.RemoveCurrent();
		}
	}
}
