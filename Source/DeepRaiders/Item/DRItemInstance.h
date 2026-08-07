// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DRItemInstance.generated.h"

class UDRItemDefinition;

USTRUCT(BlueprintType)
struct FDRItemInstance
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UDRItemDefinition> Definition = nullptr;
	
	UPROPERTY(BlueprintReadOnly)
	FGuid InstanceId;
	
	UPROPERTY(BlueprintReadOnly)
	int32 Quantity = 1;
	
	const UDRItemDefinition* GetDefinition() const
	{
		return Definition;
	}
	
	bool IsValid() const
	{
		return ::IsValid(Definition) && Quantity > 0;
	}
};