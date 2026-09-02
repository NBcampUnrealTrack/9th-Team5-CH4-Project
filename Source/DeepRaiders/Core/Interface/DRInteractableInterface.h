// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DeepRaiders/Core/Interaction/DRInteractionTypes.h"
#include "DRInteractableInterface.generated.h"

class APawn;

UINTERFACE(MinimalAPI)
class UDRInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class DEEPRAIDERS_API IDRInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(APawn* Interactor) const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool Interact(APawn* Interactor);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool GetInteractionPromptData(APawn* Interactor, FDRInteractionPromptData& OutPromptData) const;
	virtual bool GetInteractionPromptData_Implementation(APawn* Interactor, FDRInteractionPromptData& OutPromptData) const;

	/**
	 * 거리 / 조준각 / LOS 검증에 사용할 대표 상호작용 위치를 제공한다.
	 *
	 * false를 반환하면 InteractionComponent가 기존 Actor Bounds 중심을 사용한다.
	 * 긴 Rope처럼 Bounds 중심이 실제 상호작용 지점과 크게 달라질 수 있는 Actor만
	 * 이 함수를 override하면 된다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool GetInteractionLocation(APawn* Interactor, FVector& OutInteractionLocation) const;
	virtual bool GetInteractionLocation_Implementation(APawn* Interactor, FVector& OutInteractionLocation) const;
};
