// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DRTeleportSubsystem.generated.h"

class ADRTeleportPoint;
class APawn;

UCLASS()
class DEEPRAIDERS_API UDRTeleportSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterTeleportPoint(ADRTeleportPoint* TeleportPoint);
	void UnregisterTeleportPoint(ADRTeleportPoint* TeleportPoint);

	// 등록 요청의 단일 진입점이다. 이후 알림, 저장, 로그를 여기서 확장한다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Teleport")
	bool TryRegisterTeleportPoint(ADRTeleportPoint* TeleportPoint, APawn* Interactor, int32 TeamId);

private:
	UPROPERTY()
	TArray<TObjectPtr<ADRTeleportPoint>> TeleportPoints;
};
