#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRTeleportComponent.generated.h"

class ADRTeleportPoint;
class FLifetimeProperty;

UCLASS(ClassGroup = (Teleport), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRTeleportComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRTeleportComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** BP 상호작용 입력에서 호출하는 텔레포트 등록 요청 진입점이다. */
	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void RequestRegisterCurrentTeleport();

	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void RequestTeleportTo(ADRTeleportPoint* DestinationTeleportPoint);

	UFUNCTION(BlueprintPure, Category = "Teleport")
	ADRTeleportPoint* GetCurrentInteractableTeleport() const { return CurrentInteractableTeleport; }

	UFUNCTION(BlueprintPure, Category = "Teleport")
	bool IsTeleportPointRegistered(ADRTeleportPoint* TeleportPoint) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetRegisteredTeleportPoints(TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	void GetRegisteredTeleportDestinations(ADRTeleportPoint* CurrentTeleportPoint, TArray<ADRTeleportPoint*>& OutTeleportPoints) const;

	void SetCurrentInteractableTeleport(ADRTeleportPoint* TeleportPoint);
	void ClearCurrentInteractableTeleport(ADRTeleportPoint* TeleportPoint);

private:
	int32 GetOwnerTeamId() const;

	UFUNCTION(Server, Reliable)
	void ServerRequestRegisterTeleport(ADRTeleportPoint* TeleportPoint);

	UFUNCTION(Server, Reliable)
	void ServerRequestTeleportTo(ADRTeleportPoint* DestinationTeleportPoint);

	UPROPERTY(Replicated)
	TObjectPtr<ADRTeleportPoint> CurrentInteractableTeleport;
};
