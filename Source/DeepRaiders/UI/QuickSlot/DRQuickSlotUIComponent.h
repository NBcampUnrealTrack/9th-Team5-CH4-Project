// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRQuickSlotUIComponent.generated.h"

class ADRPlayerController;
class UDRQuickSlotWidget;

UCLASS()
class DEEPRAIDERS_API UDRQuickSlotUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDRQuickSlotUIComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot|UI")
	TSubclassOf<UDRQuickSlotWidget> QuickSlotWidgetClass;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UDRQuickSlotWidget> QuickSlotWidget;
};
