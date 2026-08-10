#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRTeleportComponent.generated.h"

class ADRTeleportPoint;

UCLASS(ClassGroup = (Teleport), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRTeleportComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRTeleportComponent();

	/** BP 상호작용 입력에서 호출하는 텔레포트 등록 요청 진입점이다. */
	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void RequestRegisterCurrentTeleport();

	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void RequestTeleportTo(ADRTeleportPoint* DestinationTeleportPoint);

	UFUNCTION(BlueprintPure, Category = "Teleport")
	ADRTeleportPoint* GetCurrentInteractableTeleport() const { return CurrentInteractableTeleport; }

	void SetCurrentInteractableTeleport(ADRTeleportPoint* TeleportPoint);
	void ClearCurrentInteractableTeleport(ADRTeleportPoint* TeleportPoint);

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestRegisterTeleport(ADRTeleportPoint* TeleportPoint);

	UFUNCTION(Server, Reliable)
	void ServerRequestTeleportTo(ADRTeleportPoint* DestinationTeleportPoint);

	int32 GetTemporaryTeamId() const { return 0; }

	UPROPERTY()
	TObjectPtr<ADRTeleportPoint> CurrentInteractableTeleport;
};
