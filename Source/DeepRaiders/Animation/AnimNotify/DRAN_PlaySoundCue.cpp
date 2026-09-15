#include "DRAN_PlaySoundCue.h"

#include "Components/SkeletalMeshComponent.h"
#include "DeepRaiders/GAS/Cues/DRGameplayCuePresentationLibrary.h"
#include "GameFramework/Actor.h"

void UDRAN_PlaySoundCue::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* OwnerActor = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(OwnerActor)
		|| OwnerActor->GetNetMode() == NM_DedicatedServer
		|| !SoundCueTag.IsValid())
	{
		return;
	}

	const FVector SoundLocation = !LocationSocketName.IsNone() && MeshComp->DoesSocketExist(LocationSocketName)
		? MeshComp->GetSocketLocation(LocationSocketName)
		: MeshComp->GetComponentLocation();

	FGameplayCueParameters Parameters;
	Parameters.Location = SoundLocation;
	Parameters.Instigator = OwnerActor;
	Parameters.EffectCauser = OwnerActor;
	UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(OwnerActor, SoundCueTag, Parameters);
}

FString UDRAN_PlaySoundCue::GetNotifyName_Implementation() const
{
	return SoundCueTag.IsValid() ? SoundCueTag.ToString() : TEXT("Play Sound Cue");
}
