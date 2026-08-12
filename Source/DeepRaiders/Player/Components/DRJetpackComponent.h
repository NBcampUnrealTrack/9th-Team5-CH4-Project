#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRJetpackComponent.generated.h"

class ADRPlayerCharacter;
class UDRCharacterMovementComponent;
class UStaticMesh;
class USoundBase;
class UAudioComponent;
class UCameraShakeBase;

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

	/** 착지 시 로컬 Prediction과 서버 상태를 정리한다. */
	void HandleLanded();

	/** PlayerState가 준비/복제된 시점에 초기 상태를 갱신한다. */
	void HandlePlayerStateReady();

	/** 현재 PlayerState의 제트팩 소유 여부를 외형에 반영한다. */
	void RefreshVisual();

	/** HUD가 사용할 제트팩 연료 비율 */
	float GetDisplayedFuelRatio() const;

	/** PlayerState에서 복제된 서버 연료값과 로컬 표시값을 보정한다. */
	void ReconcileFuelFromServer(float ServerFuel);
	
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

	/** 서버가 제트팩 사용을 거절하고 권위 Fuel을 알려준다. */
	UFUNCTION(Client, Reliable)
	void ClientRejectJetpack(float AuthoritativeFuel);

	UFUNCTION()
	void OnRep_JetpackActive();

	void RefreshActivePresentation();

	void StopLocalPrediction();
	
	void InitializeFuelDisplayFromServer();

	void ApplyServerFuelSnapshot(
		float ServerFuel,
		bool bSnapImmediately = false);

	void UpdateFuelInterpolation(
		float DeltaTime);
	
private:
	UPROPERTY(
		ReplicatedUsing = OnRep_JetpackActive,
		VisibleAnywhere,
		Category = "Jetpack")
	bool bIsJetpackActive = false;
	
	/** HUD에 실제 표시할 Fuel */
	float DisplayedFuel = 0.f;

	/** 직전에 받은 서버 Fuel */
	float PreviousServerFuel = 0.f;

	/** 가장 최근에 받은 서버 Fuel */
	float CurrentServerFuel = 0.f;

	/**
	 * 새로운 Snapshot을 받았을 때
	 * 실제 Lerp를 시작할 표시값.
	 */
	float FuelLerpStartValue = 0.f;

	float FuelLerpElapsed = 0.f;

	float FuelLerpDuration = 0.1f;

	/** 마지막 서버 Fuel Snapshot 수신 시간 */
	float LastFuelSnapshotTime = -1.f;

	bool bHasServerFuelSnapshot = false;
	
protected:
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Jetpack",
		meta = (ClampMin = "0.0"))
	float FuelConsumptionPerSecond = 20.f;
	
	/** 등에 표시할 제트팩 Mesh */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Jetpack|Visual")
	TObjectPtr<UStaticMesh> JetpackMesh;

	/** 등 Socket 기준 제트팩 Transform */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Jetpack|Visual")
	FTransform JetpackRelativeTransform;

	/** 제트팩 사용 중 Loop Sound */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Jetpack|Presentation")
	TObjectPtr<USoundBase> JetpackSound;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> JetpackAudioComponent;

	/** 로컬 사용 중 Camera Shake */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Jetpack|Presentation")
	TSubclassOf<UCameraShakeBase>
		JetpackCameraShakeClass;

	UPROPERTY(Transient)
	TObjectPtr<UCameraShakeBase>
		JetpackCameraShakeInstance;
};