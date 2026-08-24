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
};
