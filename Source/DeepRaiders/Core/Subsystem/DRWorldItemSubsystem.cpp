// Fill out your copyright notice in the Description page of Project Settings.


#include "DRWorldItemSubsystem.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"


ADRWorldItemActor* UDRWorldItemSubsystem::SpawnWorldItemFromDefinition(UDRItemDefinition* Definition, const FTransform& BaseSpawnTransform
	, int32 Quantity /*=1*/)
{
	UWorld* World = GetWorld();
	
	if (!World 
		|| !World->IsGameWorld()
		|| World->GetNetMode() == ENetMode::NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] : SpawnWorldItemDefinition must run on server"), *GetName());
		
		return nullptr;
	}
	
	if (!IsValid(Definition))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] : Invalid Definition"), *GetName());
		
		return nullptr;
	}
	
	if (Quantity <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] : invalid Quantity=%d"), *GetName(), Quantity);
		
		return nullptr;
	}
	
	// 새로운 ItemInstance 생성
	FDRItemInstance NewInstance;
	NewInstance.Definition =  Definition;
	NewInstance.InstanceId = FGuid::NewGuid();
	NewInstance.Quantity = FMath::Min(Quantity, Definition->MaxStackSize);
	
	return SpawnWorldItem(NewInstance, BaseSpawnTransform);
}

ADRWorldItemActor* UDRWorldItemSubsystem::SpawnWorldItem(const FDRItemInstance& ItemInstance, 
	const FTransform& BaseSpawnTransform)
{
	UWorld* World = GetWorld();
	
	if (!World
		|| !World->IsGameWorld() 
		|| World->GetNetMode() == ENetMode::NM_Client)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] : Invalid function call."), *GetName());
		
		return nullptr;
	}
	
	if (!ItemInstance.IsValid()
		|| !ItemInstance.InstanceId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] : Invalid ItemInstance."), *GetName());
		
		return nullptr;
	}
	
	// ItemDefinition에 작성된 OffsetTransform 적용
	const UDRItemDefinition* Definition = ItemInstance.Definition;
	const FTransform FinalSpawnTransform = Definition->OffsetTransform * BaseSpawnTransform;
	
	// 충돌에도 항상 생성하도록 설정
	// 추후 플러그인 추가되면 생성 방식 조정 필요할 수 있음
	ADRWorldItemActor* ItemActor = World->SpawnActorDeferred<ADRWorldItemActor>(ADRWorldItemActor::StaticClass(),
		FinalSpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	
	if (!ItemActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] : Actor Spawn failed."), *GetName());
		
		return nullptr;
	}
	
	if (!ItemActor->SetInitialItemInstance(ItemInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] : Set InitialItem Function failed."), *GetName());
		
		return nullptr;
	}
	
	UGameplayStatics::FinishSpawningActor(ItemActor, FinalSpawnTransform);
	
	ItemActor->ForceNetUpdate();
	
	return ItemActor;
}
