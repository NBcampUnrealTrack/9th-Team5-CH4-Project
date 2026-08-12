#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRJetpackComponent.generated.h"

class ADRPlayerCharacter;
class UDRCharacterMovementComponent;

UCLASS(
	ClassGroup = (Player),
	meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRJetpackComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UDRJetpackComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction)
		override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps)
		const override;

	/** 소유 클라이언트가 제트팩 시작을 요청한다. */
	void RequestStart();

	/** 소유 클라이언트가 제트팩 종료를 요청한다. */
	void RequestStop();

	/**
	 * 착지 / 사망 등 서버 내부 상황에서
	 * 제트팩을 강제로 종료한다.
	 */
	void StopFromServer();

	bool IsActive() const
	{
		return bIsJetpackActive;
	}

	float GetFuelConsumptionPerSecond() const
	{
		return FuelConsumptionPerSecond;
	}

private:
	ADRPlayerCharacter* GetOwnerCharacter() const;

	UDRCharacterMovementComponent*
		GetDRMovementComponent() const;

	bool CanStartJetpack() const;

	void StartFromServer();

	void UpdateFuel(float DeltaSeconds);

	UFUNCTION(Server, Reliable)
	void ServerStartJetpack();

	UFUNCTION(Server, Reliable)
	void ServerStopJetpack();

	/** 서버가 사용 요청을 거절하거나 연료가 소진됨. */
	UFUNCTION(Client, Reliable)
	void ClientRejectJetpack();

	UFUNCTION()
	void OnRep_JetpackActive();

private:
	UPROPERTY(
		ReplicatedUsing = OnRep_JetpackActive,
		VisibleAnywhere,
		Category = "Jetpack")
	bool bIsJetpackActive = false;

protected:
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Jetpack",
		meta = (ClampMin = "0.0"))
	float FuelConsumptionPerSecond = 20.f;
};