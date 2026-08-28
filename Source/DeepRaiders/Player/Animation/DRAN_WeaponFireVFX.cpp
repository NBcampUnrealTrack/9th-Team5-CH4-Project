#include "DRAN_WeaponFireVFX.h"

#include "Components/SkeletalMeshComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

void UDRAN_WeaponFireVFX::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(MeshComp->GetOwner());

	if (!IsValid(Character) || Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	Character->PlayProjectileFireVFXFromNotify();
}