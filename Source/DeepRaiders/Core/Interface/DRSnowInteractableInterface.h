#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "UObject/Interface.h"
#include "DRSnowInteractableInterface.generated.h"

UINTERFACE(BlueprintType)
class UDRSnowInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class DEEPRAIDERS_API IDRSnowInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snow|Interaction")
	bool CanReceiveSnowAdd(const FDRSnowSurfaceAddRequest& Request) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snow|Interaction")
	void ReceiveSnowAdded(const FDRSnowSurfaceAddRequest& Request);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snow|Interaction")
	bool CanReceiveSnowRemove(const FDRSnowSurfaceRemoveRequest& Request) const;

	// 실제 제거된 눈 양을 반환한다. 눈이 없으면 0을 반환해야 한다.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snow|Interaction")
	float ReceiveSnowRemoved(const FDRSnowSurfaceRemoveRequest& Request);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snow|Interaction")
	bool CanReceiveSnowDamage(const FDRSnowDamageRequest& Request) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Snow|Interaction")
	void ReceiveSnowDamage(const FDRSnowDamageRequest& Request);
};
