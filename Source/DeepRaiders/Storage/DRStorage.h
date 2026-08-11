// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DRStorage.generated.h"

class UDRInventoryComponent;

UCLASS()
class DEEPRAIDERS_API ADRStorage : public AActor, public IDRInteractableInterface
{
	GENERATED_BODY()
	
public:	
	ADRStorage();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<USceneComponent> Root;
	
	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<UDRInventoryComponent> InventoryComponent;
	
public:
	UFUNCTION(BlueprintPure, Category = "Storage")
	UDRInventoryComponent* GetInventoryComponent() const
	{
		return InventoryComponent.Get();
	}
	
#pragma region Interact
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(APawn* Interactor) const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool Interact(APawn* Interactor);
#pragma endregion

};
