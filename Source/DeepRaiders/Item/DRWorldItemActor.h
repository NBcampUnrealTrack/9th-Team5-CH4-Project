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
	
	const FDRItemInstance& GetItemInstance() const
	{
		return ItemInstance;
	}
	
	bool SetInitialItemInstance(FDRItemInstance InItemInstance);
	
protected:	
	UFUNCTION()
	void OnRep_ItemInstance();
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ItemInstance)
	FDRItemInstance ItemInstance;
	
private:
	// ItemInstance 갱신 시마다 호출
	// MeshData 갱신
	void RefreshItemPresentation();
	
};
