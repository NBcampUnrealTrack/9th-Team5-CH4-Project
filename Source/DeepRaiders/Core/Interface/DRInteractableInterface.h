// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
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
};
