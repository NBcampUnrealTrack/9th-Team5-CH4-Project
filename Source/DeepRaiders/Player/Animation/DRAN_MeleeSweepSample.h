#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DRAN_MeleeSweepSample.generated.h"

UCLASS()
class DEEPRAIDERS_API UDRAN_MeleeSweepSample
	: public UAnimNotify
{
	GENERATED_BODY()

public:
	UDRAN_MeleeSweepSample();

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};