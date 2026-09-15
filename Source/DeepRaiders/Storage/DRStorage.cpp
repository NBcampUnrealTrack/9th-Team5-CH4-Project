// Fill out your copyright notice in the Description page of Project Settings.


#include "DRStorage.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
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
	
	DOREPLIFETIME(ThisClass, StorageOwner);
}

bool ADRStorage::TrySetStorageOwner(AActor* NewOwner)
{
	if (!HasAuthority()
		|| !IsValid(NewOwner) || NewOwner->GetWorld() != GetWorld()
		|| StorageOwner == NewOwner)
	{
		return false;
	}
	
	AActor* PreviousOwner = StorageOwner.Get();
	
	StorageOwner = NewOwner;
	OnStorageOwnerChangedDelegate.Broadcast(PreviousOwner, NewOwner);
	
	ADRPlayerCharacter* DRPlayerCharacter = Cast<ADRPlayerCharacter>(NewOwner);
	if (DRPlayerCharacter)
	{
		DRPlayerCharacter->OnPlayerCharacterDeathDelegate.AddDynamic(this, &ThisClass::HandleOwnerActorDeath);
	}
	
	FlushNetDormancy();
	ForceNetUpdate();
	
	return true;	
}

bool ADRStorage::TryClaimOwnership(AActor* NewOwner)
{
	if (!HasAuthority()
		|| !IsValid(NewOwner))
	{
		return false;
	}
	
	if (Authority == EDRStorageAuthority::Common)
	{
		return true;
	}
	
	AActor* CurrentOwner = StorageOwner.Get();
	
	if (IsValid(CurrentOwner))
	{
		return CurrentOwner == NewOwner;
	}
	
	return TrySetStorageOwner(NewOwner);
}

bool ADRStorage::TryReleaseStorageOwner()
{
	if (!HasAuthority()
		|| !IsValid(StorageOwner.Get()))
	{
		return false;
	}
	
	ADRPlayerCharacter* DRPlayerCharacter = Cast<ADRPlayerCharacter>(StorageOwner);
	if (DRPlayerCharacter)
	{
		DRPlayerCharacter->OnPlayerCharacterDeathDelegate.RemoveDynamic(this, &ThisClass::HandleOwnerActorDeath);
	}
	
	AActor* PreviousOwner = StorageOwner.Get();
	
	StorageOwner = nullptr;
	OnStorageOwnerChangedDelegate.Broadcast(PreviousOwner, nullptr);
	
	FlushNetDormancy();
	ForceNetUpdate();
	
	return true;
}

bool ADRStorage::IsOwnerBy(AActor* InActor) const
{
	if (Authority == EDRStorageAuthority::Common)
		return true;
	
	return IsValid(InActor) && StorageOwner == InActor;
}

void ADRStorage::OnRep_StorageOwner(TWeakObjectPtr<AActor> PreviousOwner)
{
	OnStorageOwnerChangedDelegate.Broadcast(PreviousOwner.Get(), StorageOwner.Get());
}

void ADRStorage::HandleOwnerActorDeath()
{
	// 함수 내부에서 OnStorageOwnerChangedDelegate 전파
	TryReleaseStorageOwner();
}

bool ADRStorage::CanInteract_Implementation(APawn* Interactor) const
{
	if (!IsValid(Interactor))
	{
		return false;
	}
	
	if (Authority == EDRStorageAuthority::Common)
		return true;
	
	AActor* InteractorState = Cast<AActor>(Interactor);
	AActor* CurrentOwner = StorageOwner.Get();
	
	// 소유권이 아무도 없거나, 주인인 경우 성공
	return IsValid(InteractorState) 
		&& (!IsValid(CurrentOwner) || IsOwnerBy(InteractorState));
}

bool ADRStorage::Interact_Implementation(APawn* Interactor)
{
	if (!Execute_CanInteract(this, Interactor))
	{
		return false;
	}
	
	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(Interactor->GetController());
	
	// DRPlayerController 리팩토링으로 인해 정상적인 사용이 불가능합니다.
	//return IsValid(PlayerController) && PlayerController->TryOpenStorage(this);
	ensure(true);
	return IsValid(PlayerController);
}
