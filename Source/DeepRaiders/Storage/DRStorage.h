// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DRStorage.generated.h"

class UDRInventoryComponent;
class ADRPlayerState;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDRStorageOwnerChanged, AActor*, PreviousOwner, AActor*, NewOwner);

UENUM()
enum class EDRStorageAuthority : uint8
{
	Common,
	Private,
	Team, // 미구현
};

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
	AActor* GetStorageOwner() const
	{
		return StorageOwner.Get();
	}
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Storage|Ownership")
	bool TrySetStorageOwner(AActor* NewOwner);
	
	// 소유권 습득 시도, 이미 소유자가 있는 경우 실패
	bool TryClaimOwnership(AActor* NewOwner);
	bool TryReleaseStorageOwner();
	
	UFUNCTION(BlueprintPure, Category = "Storage|Ownership")
	bool IsOwnerBy(AActor* InPlayerState) const;

	UPROPERTY(BlueprintAssignable, Category = "Storage|Ownership")
	FDRStorageOwnerChanged OnStorageOwnerChangedDelegate;
	
protected:	
	UFUNCTION()
	void OnRep_StorageOwner(TWeakObjectPtr<AActor> PreviousOwner);
	
private:
	UFUNCTION()
	void HandleOwnerActorDeath();
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<USceneComponent> Root;
	
	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<UDRInventoryComponent> InventoryComponent;
	
	UPROPERTY(ReplicatedUsing = OnRep_StorageOwner, VisibleInstanceOnly, BlueprintReadOnly, Category = "Storage|Ownership")
	TWeakObjectPtr<AActor> StorageOwner;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage")
	EDRStorageAuthority Authority = EDRStorageAuthority::Common;
	
#pragma region Interact
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(APawn* Interactor) const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool Interact(APawn* Interactor);
#pragma endregion

};
