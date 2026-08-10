// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DRTeleportSubsystem.generated.h"

class ADRTeleportPoint;
class APawn;

USTRUCT()
struct FDRTeleportPointList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<ADRTeleportPoint>> TeleportPoints;
};

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

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetPublicRegisteredTeleportPoints(TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetTeamRegisteredTeleportPoints(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetRegisteredTeleportPointsForTeam(int32 TeamId, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetRegisteredTeleportDestinationsForTeam(int32 TeamId, ADRTeleportPoint* CurrentTeleportPoint, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	bool CanUseRegisteredTeleportPoint(int32 TeamId, ADRTeleportPoint* TeleportPoint) const;

private:
	void AddRegisteredTeleportPoint(ADRTeleportPoint* TeleportPoint, int32 TeamId);
	void RemoveRegisteredTeleportPoint(ADRTeleportPoint* TeleportPoint);
	void AppendValidTeleportPoints(const TArray<TObjectPtr<ADRTeleportPoint>>& Source, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	UPROPERTY()
	TArray<TObjectPtr<ADRTeleportPoint>> TeleportPoints;

	UPROPERTY()
	TArray<TObjectPtr<ADRTeleportPoint>> PublicRegisteredTeleports;

	UPROPERTY()
	TMap<int32, FDRTeleportPointList> TeamRegisteredTeleports;
};
