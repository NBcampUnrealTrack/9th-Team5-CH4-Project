// Fill out your copyright notice in the Description page of Project Settings.


#include "DRStorage.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ADRStorage::ADRStorage()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	
	InventoryComponent = CreateDefaultSubobject<UDRInventoryComponent>(TEXT("InventoryComponent"));	
}

void ADRStorage::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, StorageOwnerPlayerState);
}

bool ADRStorage::TrySetStorageOwner(APlayerState* NewOwner)
{
	if (!HasAuthority()
		|| !IsValid(NewOwner) || NewOwner->GetWorld() != GetWorld()
		|| StorageOwnerPlayerState == NewOwner)
	{
		return false;
	}
	
	APlayerState* PreviousOwner = StorageOwnerPlayerState.Get();
	
	StorageOwnerPlayerState = NewOwner;
	OnStorageOwnerChangedDelegate.Broadcast(PreviousOwner, NewOwner);
	
	FlushNetDormancy();
	ForceNetUpdate();
	
	return true;	
}

bool ADRStorage::TryClaimOwnership(APlayerState* InPlayerState)
{
	if (!HasAuthority()
		|| !IsValid(InPlayerState))
	{
		return false;
	}
	
	APlayerState* CurrentOwner = StorageOwnerPlayerState.Get();
	
	if (IsValid(CurrentOwner))
	{
		return CurrentOwner == InPlayerState;
	}
	
	return TrySetStorageOwner(InPlayerState);
}

bool ADRStorage::TryReleaseStorageOwner()
{
	if (!HasAuthority()
		|| !IsValid(StorageOwnerPlayerState))
	{
		return false;
	}
	
	APlayerState* PreviousOwner = StorageOwnerPlayerState.Get();
	
	StorageOwnerPlayerState = nullptr;
	
	OnStorageOwnerChangedDelegate.Broadcast(PreviousOwner, nullptr);
	
	FlushNetDormancy();
	ForceNetUpdate();
	
	return true;
}

bool ADRStorage::IsOwnerBy(APlayerState* InPlayerState) const
{
	return IsValid(InPlayerState) && StorageOwnerPlayerState == InPlayerState;
}

void ADRStorage::OnRep_StorageOwner(APlayerState* PreviousOwner)
{
	OnStorageOwnerChangedDelegate.Broadcast(PreviousOwner, StorageOwnerPlayerState.Get());
}

bool ADRStorage::CanInteract_Implementation(APawn* Interactor) const
{
	if (!IsValid(Interactor))
	{
		return false;
	}
	
	APlayerState* InteractorState = Interactor->GetPlayerState();
	APlayerState* CurrentOwner = StorageOwnerPlayerState.Get();
	
	// 소유권이 아무도 없거나, 주인인 경우 성공
	return IsValid(InteractorState) 
		&& (!IsValid(CurrentOwner) || IsOwnerBy(InteractorState));
}

bool ADRStorage::Interact_Implementation(APawn* Interactor)
{
	if (!CanInteract(Interactor))
	{
		return false;
	}
	
	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(Interactor->GetController());
	
	return IsValid(PlayerController) && PlayerController->TryOpenStorage(this);
}
