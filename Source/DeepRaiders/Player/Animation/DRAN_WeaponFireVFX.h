#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DRAN_WeaponFireVFX.generated.h"

UCLASS(meta = (DisplayName = "DRAN_WeaponFireVFX"))
class DEEPRAIDERS_API UDRAN_WeaponFireVFX
	: public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};