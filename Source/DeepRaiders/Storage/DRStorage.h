// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DRStorage.generated.h"

class UDRInventoryComponent;
class ADRPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDRStorageOwnerChanged, APlayerState*, PreviousOwner, APlayerState*, NewOwner);

UCLASS()
class DEEPRAIDERS_API ADRStorage : public AActor, public IDRInteractableInterface
{
	GENERATED_BODY()
	
public:	
	ADRStorage();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Storage")
	UDRInventoryComponent* GetInventoryComponent() const
	{
		return InventoryComponent.Get();
	}
	
	UFUNCTION(BlueprintPure, Category = "Storage")
	APlayerState* GetStorageOwner() const
	{
		return StorageOwnerPlayerState.Get();
	}
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Storage|Ownership")
	bool TrySetStorageOwner(APlayerState* NewOwner);
	
	// 소유권 습득 시도, 이미 소유자가 있는 경우 실패
	bool TryClaimOwnership(APlayerState* InPlayerState);
	bool TryReleaseStorageOwner();
	
	UFUNCTION(BlueprintPure, Category = "Storage|Ownership")
	bool IsOwnerBy(APlayerState* InPlayerState) const;

	UPROPERTY(BlueprintAssignable, Category = "Storage|Ownership")
	FDRStorageOwnerChanged OnStorageOwnerChangedDelegate;
	
protected:	
	UFUNCTION()
	void OnRep_StorageOwner(APlayerState* PreviousOwner);
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<USceneComponent> Root;
	
	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<UDRInventoryComponent> InventoryComponent;
	
	UPROPERTY(ReplicatedUsing = OnRep_StorageOwner, VisibleInstanceOnly, BlueprintReadOnly, Category = "Storage|Ownership")
	TObjectPtr<APlayerState> StorageOwnerPlayerState;

#pragma region Interact
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(APawn* Interactor) const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool Interact(APawn* Interactor);
#pragma endregion

};
