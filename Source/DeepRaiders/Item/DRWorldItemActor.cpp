// Fill out your copyright notice in the Description page of Project Settings.


#include "DRWorldItemActor.h"
#include "DRItemInstance.h"
#include "DRItemDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"

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
	StaticMeshComponent->BodyInstance.bStartAwake = false;
}

void ADRWorldItemActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HasAuthority())
	{
		// 클라이언트는 OnRep_ItemInstance에서 초기화 됨
		return;
	}
	
	if (!ItemInstance.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Spawned without Iteminstance"), *GetName());

		Destroy();
		return;
	}
	
	RefreshItemPresentation();
	
	if (StaticMeshComponent->IsSimulatingPhysics())
	{
		StaticMeshComponent->PutRigidBodyToSleep();
	}
}

void ADRWorldItemActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, ItemInstance);
}

bool ADRWorldItemActor::SetInitialItemInstance(FDRItemInstance InItemInstance)
{
	if (!HasAuthority())
	{
		return false;
	}
	
	// ItemInstance 설정은 BeginPlay 전에 완료되어야 함.
	if (HasActorBegunPlay())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] ItemInstance must be assigned before BeginPlay"), *GetName());
		
		return false;
	}
	
	if (ItemInstance.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] ItemInstance is already assigned."), *GetName());
		
		return false;
	}
	
	if (!InItemInstance.IsValid()
		|| !InItemInstance.InstanceId.IsValid())
	{
		return false;
	}
	
	ItemInstance = InItemInstance;
	
	return true;
}

void ADRWorldItemActor::ApplyDropImpulse(const FVector& Impulse)
{
	if (!HasAuthority()
		|| !IsValid(StaticMeshComponent)
		|| !StaticMeshComponent->IsSimulatingPhysics())
	{
		return;
	}
	
	StaticMeshComponent->WakeAllRigidBodies();
	StaticMeshComponent->AddImpulse(Impulse, NAME_None, true);
}

bool ADRWorldItemActor::IsPickupAvailable() const
{
	// 기본적으로 모든 WorldItemActor는 인벤토리에 넣을 수 있다.
	return true;
}

bool ADRWorldItemActor::FinalizePickup()
{
	return Destroy();
}

void ADRWorldItemActor::ResetInteractionState()
{
	bInteractionInProgress = false;
}

void ADRWorldItemActor::OnRep_ItemInstance()
{
	RefreshItemPresentation();
	
	const FRepMovement& RepMovement = GetReplicatedMovement();
	
	if (RepMovement.bRepPhysics 
		&& RepMovement.bSimulatedPhysicSleep
		&& StaticMeshComponent->IsSimulatingPhysics())
	{
		StaticMeshComponent->PutRigidBodyToSleep();
	}
}

void ADRWorldItemActor::RefreshItemPresentation()
{
	const UDRItemDefinition* Definition = ItemInstance.GetDefinition();
	
	if (Definition == nullptr
		|| !ItemInstance.IsValid())
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

#pragma region Interactable
bool ADRWorldItemActor::CanInteract_Implementation(APawn* Interactor) const
{
	const ADRPlayerController* Controller = IsValid(Interactor) ? Cast<ADRPlayerController>(Interactor->GetController()) : nullptr;
	
	return HasAuthority() && !bInteractionInProgress &&  ItemInstance.IsValid()
		&& IsValid(Controller) && IsPickupAvailable() && Controller->CanReceiveItem(ItemInstance.Definition, ItemInstance.Quantity);
}

bool ADRWorldItemActor::Interact_Implementation(APawn* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return false;
	}
	
	ADRPlayerController* Controller = Cast<ADRPlayerController>(Interactor->GetController());
	
	bInteractionInProgress = true;
	if (!Controller->TryReceiveItem(ItemInstance.Definition, ItemInstance.Quantity))
	{
		bInteractionInProgress = false;
		return false;
	}
	
	if (!FinalizePickup())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Failed to finalize pickup."), *GetName());
		
		// 이미 인벤토리에 추가되었기 때문에 상호작용은 그대로 불가능
		return false;
	}
	
	return true;
}
#pragma endregion

