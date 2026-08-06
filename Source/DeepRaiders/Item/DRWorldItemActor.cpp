// Fill out your copyright notice in the Description page of Project Settings.


#include "DRWorldItemActor.h"
#include "DRItemInstance.h"
#include "DRItemDefinition.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ADRWorldItemActor::ADRWorldItemActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	bReplicates = true;
	SetReplicateMovement(true);
	
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	SetRootComponent(StaticMeshComponent);
	
	StaticMeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	StaticMeshComponent->SetSimulatePhysics(true);
	
	ConstructorHelpers::FObjectFinder<UDRItemDefinition> ItemDefinitionAssetRef(TEXT("/Script/DeepRaiders.DRItemDefinition'/Game/DeepRaiders/Data/DataAssets/DA_DRTestItemDefinition.DA_DRTestItemDefinition'"));
	if (ItemDefinitionAssetRef.Object)
	{
		DefaultItemDefinition = ItemDefinitionAssetRef.Object;
	}
}

void ADRWorldItemActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HasAuthority())
	{
		return;
	}
	
	if (ItemInstance.IsValid())
	{
		return;
	}
	
	if (!IsValid(DefaultItemDefinition))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] DefaultItemDefinition is null"), *GetName());
		
		return;
	}
	
	InitializeItemFromDefinition(DefaultItemDefinition, DefaultItemQuantity);
}

void ADRWorldItemActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, ItemInstance);
}

void ADRWorldItemActor::InitializeItem(const FDRItemInstance& InItemInstance)
{
	if (!HasAuthority())
	{
		return;
	}
	
	ItemInstance = InItemInstance;
	RefreshItemPresentation();
	ForceNetUpdate();
	
	UE_LOG(LogTemp, Error, TEXT("[InitializeItem] Actor=%s Definition=%s Quantity=%d")
		, *GetName(), *GetNameSafe(ItemInstance.Definition), ItemInstance.Quantity);
}

void ADRWorldItemActor::InitializeItemFromDefinition(UDRItemDefinition* InDefinition, int32 InQuantity)
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (!IsValid(InDefinition))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Invalid Item Definition"), *GetName());
		
		return;
	}
	
	if (InQuantity <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Invalid Item Quantity : %d"), *GetName(), InQuantity);
		
		return;
	}
	
	FDRItemInstance NewItemInstance;
	NewItemInstance.Definition = InDefinition;
	NewItemInstance.InstanceId = FGuid::NewGuid();
	NewItemInstance.Quantity = FMath::Min(InQuantity, InDefinition->MaxStackSize);
	
	InitializeItem(NewItemInstance);
}

void ADRWorldItemActor::OnRep_ItemInstance()
{
	RefreshItemPresentation();
	
	UE_LOG(LogTemp, Error, TEXT("[OnRep_ItemInstance] Actor=%s Definition=%s Quantity=%d")
	, *GetName(), *GetNameSafe(ItemInstance.Definition), ItemInstance.Quantity);
}

void ADRWorldItemActor::RefreshItemPresentation()
{
	const UDRItemDefinition* Definition = ItemInstance.GetDefinition();
	
	if (!ItemInstance.IsValid())
	{
		StaticMeshComponent->SetStaticMesh(nullptr);
		StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		
		SetActorHiddenInGame(true);
		return;
	}
	
	StaticMeshComponent->SetStaticMesh(Definition->WorldMesh);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	SetActorHiddenInGame(false);	
}
