// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRQuickSlotUIComponent.generated.h"

class ADRPlayerController;
class UDRQuickSlotWidget;
class UDRUIManagerSubsystem;

UCLASS()
class DEEPRAIDERS_API UDRQuickSlotUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDRQuickSlotUIComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UDRUIManagerSubsystem> UIManager;

	UPROPERTY(Transient)
	TObjectPtr<UDRQuickSlotWidget> QuickSlotWidget;
};
