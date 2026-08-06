// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRItemInstance.h"
#include "DRWorldItemActor.generated.h"

UCLASS()
class DEEPRAIDERS_API ADRWorldItemActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ADRWorldItemActor();
	
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	void InitializeItem(const FDRItemInstance& InItemInstance);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item")
	void InitializeItemFromDefinition(UDRItemDefinition* InDefinition, int32 InQuantity = 1);
	
	const FDRItemInstance& GetItemInstance() const
	{
		return ItemInstance;
	}
	
protected:	
	UFUNCTION()
	void OnRep_ItemInstance();
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ItemInstance)
	FDRItemInstance ItemInstance;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UDRItemDefinition> DefaultItemDefinition = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 DefaultItemQuantity = 1;
	
private:
	void RefreshItemPresentation();
	
};
