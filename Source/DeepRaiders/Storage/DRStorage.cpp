// Fill out your copyright notice in the Description page of Project Settings.


#include "DRStorage.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "GameFramework/Pawn.h"

ADRStorage::ADRStorage()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	
	InventoryComponent = CreateDefaultSubobject<UDRInventoryComponent>(TEXT("InventoryComponent"));	
}

bool ADRStorage::CanInteract_Implementation(APawn* Interactor) const
{
	// 추후 팀 선정, 권한 추가 시 수정 필요
	return IsValid(Interactor);
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
