// Fill out your copyright notice in the Description page of Project Settings.


#include "DRWorldItemSubsystem.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Item/DRHoveringWorldItemActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"


ADRWorldItemActor* UDRWorldItemSubsystem::SpawnWorldItemFromDefinition(UDRItemDefinition* Definition,
	const FTransform& BaseSpawnTransform, int32 Quantity /*=1*/)
{
	FDRWorldItemSpawnParams SpawnParams;
	SpawnParams.SourceTransform = BaseSpawnTransform;
	SpawnParams.TargetTransform = BaseSpawnTransform;
	SpawnParams.bPlayEmergence = false;

	return SpawnWorldItemFromDefinitionWithParams(Definition, SpawnParams, Quantity);
}

ADRWorldItemActor* UDRWorldItemSubsystem::SpawnWorldItem(const FDRItemInstance& ItemInstance, 
	const FTransform& BaseSpawnTransform)
{
	FDRWorldItemSpawnParams SpawnParams;
	SpawnParams.SourceTransform = BaseSpawnTransform;
	SpawnParams.TargetTransform = BaseSpawnTransform;
	SpawnParams.bPlayEmergence = false;

	return SpawnWorldItem(ItemInstance, SpawnParams);
}

ADRWorldItemActor* UDRWorldItemSubsystem::SpawnWorldItemFromDefinitionWithParams(UDRItemDefinition* Definition,
	const FDRWorldItemSpawnParams& SpawnParams, int32 Quantity)
{
	UWorld* World = GetWorld();
	
	if (!IsValid(World)
		|| !World->IsGameWorld()
		|| World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: World item spawning must run on the server."), *GetName());
		
		return nullptr;
	}
	
	const FDRItemInstance ItemInstance = DRItemInstanceFactory::Create(Definition, Quantity);
	
	return ItemInstance.IsValid() ? SpawnWorldItem(ItemInstance, SpawnParams) : nullptr;
}

ADRWorldItemActor* UDRWorldItemSubsystem::SpawnWorldItem(const FDRItemInstance& ItemInstance,
	const FDRWorldItemSpawnParams& SpawnParams)
{
	UWorld* World = GetWorld();
	
	if (!IsValid(World)
		|| !World->IsGameWorld()
		|| World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: Invalid world item spawn call."), *GetName());
		
		return nullptr;
	}
	
	if (!ItemInstance.IsValid()
		|| !ItemInstance.InstanceId.IsValid()
		|| !IsValid(ItemInstance.Definition))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: Invalid ItemInstance."), *GetName());
		
		return nullptr;
	}
	
	const UDRItemDefinition* Definition = ItemInstance.Definition;
	
	// 기획 변경으로 인해 호버링 액터를 기본으로 사용
	TSubclassOf<ADRWorldItemActor> SpawnActorClass = ADRHoveringWorldItemActor::StaticClass();
	if (Definition->ActorClass)
	{
		SpawnActorClass = Definition->ActorClass;
	}
	
	const bool bUsesHoverPresentation = SpawnActorClass.Get()->IsChildOf(ADRHoveringWorldItemActor::StaticClass());
	const FTransform SourceWorldTransform = Definition->SpawnOffsetTransform * SpawnParams.SourceTransform;
	const FTransform SpawnTransform = bUsesHoverPresentation ? 
		ResolveHoverSpawnTransform(Definition, SpawnParams) : Definition->SpawnOffsetTransform * SpawnParams.TargetTransform;
	
	ADRWorldItemActor* ItemActor = World->SpawnActorDeferred<ADRWorldItemActor>(SpawnActorClass, SpawnTransform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	
	if (!IsValid(ItemActor))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s]: World item Actor spawn failed."), *GetName());
		
		return nullptr;
	}
	
	if (!ItemActor->SetInitialItemInstance(ItemInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Failed to initialize ItemInstance"), *GetName());
		
		ItemActor->Destroy();
		return nullptr;
	}
	
	if (ADRHoveringWorldItemActor* HoveringItem = Cast<ADRHoveringWorldItemActor>(ItemActor))
	{
		if (!HoveringItem->InitializeHoverPresentation(SourceWorldTransform.GetLocation(), SpawnParams.bPlayEmergence))
		{
			UE_LOG(LogTemp, Error, TEXT("[%s]: Failed to initialize hover presentation."), *GetName());
			
			ItemActor->Destroy();
			return nullptr;
		}
	}
	else if (SpawnParams.bPlayEmergence)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s]: %s does not support hover presentation."),
			*GetName(),	*GetNameSafe(SpawnActorClass.Get()));
	}
	
	UGameplayStatics::FinishSpawningActor(ItemActor, SpawnTransform);
	ItemActor->ForceNetUpdate();
	
	return ItemActor;	
}

FTransform UDRWorldItemSubsystem::ResolveHoverSpawnTransform(const UDRItemDefinition* Definition,
	const FDRWorldItemSpawnParams& SpawnParams) const
{
	constexpr float PathSweepRadius = 20.f;
	constexpr float GroundSweepRadius = 10.f;
	constexpr float GroundTraceStartOffset = 100.f;
	constexpr float GroundTraceDistance = 500.f;
	constexpr float HoverGroundClearance = 30.f;
	
	FTransform DesiredTransform = Definition->SpawnOffsetTransform * SpawnParams.TargetTransform;
	const FTransform SourceTransform = Definition->SpawnOffsetTransform * SpawnParams.SourceTransform;
	
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return DesiredTransform;
	}
	
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRWorldItemSpawn), false);
	
	if (IsValid(SpawnParams.IgnoredActor))
	{
		QueryParams.AddIgnoredActor(SpawnParams.IgnoredActor);
	}
	
	FVector DesiredLocation = DesiredTransform.GetLocation();
	const FVector SourceLocation= SourceTransform.GetLocation();
	
	if (!SourceLocation.Equals(DesiredLocation, KINDA_SMALL_NUMBER))
	{
		FHitResult PathHit;
		
		// 목표까지의 경로가 막히지 않았는지 검사
		if (World->SweepSingleByChannel(PathHit, SourceLocation, DesiredLocation, FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(PathSweepRadius), QueryParams))
		{
			DesiredLocation = PathHit.Location;
			DesiredTransform.SetLocation(DesiredLocation);
		}
	}
	
	const FVector GroundTraceStart = DesiredLocation + FVector::UpVector * GroundTraceStartOffset;
	const FVector GroundTraceEnd = DesiredLocation - FVector::UpVector * GroundTraceDistance;
	
	FHitResult GroundHit;
	
	// 지면 검사, 생성 위치부터 지면까지 너무 멀다면 높이를 수정하지 않는다.
	if (!World->SweepSingleByChannel(GroundHit, GroundTraceStart, GroundTraceEnd, FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(GroundSweepRadius), QueryParams))
	{
		return DesiredTransform;
	}
	
	float ActorOriginToMeshBottom = 0.f;
	
	// Mesh의 높이만큼 위치 보정
	if (IsValid(Definition->WorldMesh))
	{
		const FBoxSphereBounds WorldBounds = Definition->WorldMesh->GetBounds().TransformBy(DesiredTransform);
		const float MeshBottom = WorldBounds.Origin.Z - WorldBounds.BoxExtent.Z;
		
		ActorOriginToMeshBottom = DesiredTransform.GetLocation().Z - MeshBottom;
	}
	
	DesiredLocation.Z = GroundHit.ImpactPoint.Z + ActorOriginToMeshBottom + HoverGroundClearance;
	DesiredTransform.SetLocation(DesiredLocation);
	
	return DesiredTransform;
}
