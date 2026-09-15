#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRHitReactionSet.generated.h"

class UAnimSequenceBase;

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRHitReactionSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimSequenceBase> Front;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimSequenceBase> Back;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimSequenceBase> Left;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimSequenceBase> Right;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction")
	FName SlotName = TEXT("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction",
		meta = (ClampMin = "0.0"))
	float BlendInTime = 0.03f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction",
		meta = (ClampMin = "0.0"))
	float BlendOutTime = 0.08f;

	// Shotgun처럼 짧은 시간에 여러 Hit가 들어와 Montage가 계속 Restart되는 것을 방지한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction",
		meta = (ClampMin = "0.0"))
	float MinReplayInterval = 0.1f;
};