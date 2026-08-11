// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DRThrowableItemInterface.generated.h"

class APawn;

UINTERFACE(MinimalAPI)
class UDRThrowableItemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class DEEPRAIDERS_API IDRThrowableItemInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Throwable")
	void NotifyThrown(APawn* Thrower);
};
