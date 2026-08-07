// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DRWorldItemSubsystem.generated.h"

class ADRWorldItemActor;
class UDRItemDefinition;

/*
 * 아이템 스폰을 담당하는 World 서브시스템
 */
UCLASS()
class DEEPRAIDERS_API UDRWorldItemSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	/*
	 * 이미 만들어진 ItemInstance를 월드 Actor로 생성
	 * 서버 전용
	 */
	ADRWorldItemActor* SpawnWorldItem(const FDRItemInstance& ItemInstance, const FTransform& BaseSpawnTransform);
	
	/*
	 * 서버가 Definition을 기반으로 새로운 ItemInstance를 생성하고 월드에 스폰시킨다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Item|World")
	ADRWorldItemActor* SpawnWorldItemFromDefinition(UDRItemDefinition* Definition, const FTransform& BaseSpawnTransform, int32 Quantity = 1);
	
};
