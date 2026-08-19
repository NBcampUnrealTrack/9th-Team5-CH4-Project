// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DRItemDefinition.h"
#include "DRItemTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "DRItemInstance.generated.h"

class UDRItemDefinition;

USTRUCT(BlueprintType)
struct FDRItemInstance
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UDRItemDefinition> Definition = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	FGuid InstanceId;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	int32 Quantity = 1;
	
	// 런타임 인스턴스가 보유 중인 특이 사항 정보
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TInstancedStruct<FDRItemRuntimeState> RuntimeState;
	
	const UDRItemDefinition* GetDefinition() const
	{
		return Definition;
	}
	
	bool IsValid() const
	{
		return ::IsValid(Definition) 
		&& InstanceId.IsValid()
		&& Quantity > 0;
	}
};

namespace DRItemInstanceFactory
{
	DEEPRAIDERS_API FDRItemInstance Create(UDRItemDefinition* Definition, int32 Quantity = 1);
}