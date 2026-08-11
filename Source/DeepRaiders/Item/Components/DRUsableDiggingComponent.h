// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "DRUsableDiggingComponent.generated.h"

class UDRUsableDiggingDefinition;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRUsableDiggingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRUsableDiggingComponent();

	UFUNCTION(BlueprintCallable, Category = "Digging")
	void StartDigging();

	UFUNCTION(BlueprintCallable, Category = "Digging")
	void StopDigging();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool CanRunDigging() const;
	void StartDiggingInternal();
	void ExecuteDig();
	bool RequestDigAtLocation(const FVector& DigLocation) const;
	void DrawDigRadiusDebug(const FVector& Center, const FColor& Color, float DrawTime) const;
	void FinishDigging();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digging")
	TObjectPtr<UDRUsableDiggingDefinition> DiggingDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digging")
	uint8 bAutoStartOnBeginPlay : 1 = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digging|Debug")
	uint8 bDrawDebugDigRadius : 1 = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Digging|Debug", meta = (ClampMin = "0.0", Units = "s"))
	float DebugExplosionDrawTime = 1.f;

private:
	FTimerHandle StartTimerHandle;
	uint8 bDiggingStarted : 1 = false;
};
