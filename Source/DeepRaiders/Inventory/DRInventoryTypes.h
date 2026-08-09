// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DRInventoryTypes.generated.h"

class UDRItemDefinition;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRInventoryEntry
{
	GENERATED_BODY()

public:	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	FGuid EntryId;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UDRItemDefinition> Definition = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;
	
	bool IsValid() const
	{
		return EntryId.IsValid() && Definition != nullptr && Quantity > 0;
	}
};
