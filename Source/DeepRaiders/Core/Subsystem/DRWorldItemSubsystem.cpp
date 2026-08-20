// Fill out your copyright notice in the Description page of Project Settings.


#include "DRWorldItemSubsystem.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Item/DRItemInstance.h"
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
	
	const FDRItemInstance ItemInstance = DRItemInstanceFactory::Create(Definition, Quantity);
	
	if (!ItemInstance.IsValid())
	{
		return nullptr;
	}
	
	return SpawnWorldItem(ItemInstance, BaseSpawnTransform);
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
	const FTransform FinalSpawnTransform = Definition->SpawnOffsetTransform * BaseSpawnTransform;
	
	TSubclassOf<ADRWorldItemActor> SpawnActorClass = ADRWorldItemActor::StaticClass();
	if (IsValid(Definition->ActorClass))
	{
		SpawnActorClass = Definition->ActorClass;
	}

	// 충돌에도 항상 생성하도록 설정
	ADRWorldItemActor* ItemActor = World->SpawnActorDeferred<ADRWorldItemActor>(SpawnActorClass,
	FinalSpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	
	if (!ItemActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] : Actor Spawn failed."), *GetName());
		
		return nullptr;
	}
	
	if (!ItemActor->SetInitialItemInstance(ItemInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] : Set InitialItem Function failed."), *GetName());
		
		ItemActor->Destroy();
		return nullptr;
	}
	
	UGameplayStatics::FinishSpawningActor(ItemActor, FinalSpawnTransform);
	
	ItemActor->ForceNetUpdate();
	
	return ItemActor;
}
