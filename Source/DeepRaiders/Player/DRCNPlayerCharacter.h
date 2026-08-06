#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "DRCNPlayerCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class FLifetimeProperty;

UCLASS()
class DEEPRAIDERS_API ADRCNPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADRCNPlayerCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void PawnClientRestart() override;

	// 복제할 프로퍼티를 엔진에 등록하는 함수
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(
		UInputComponent* PlayerInputComponent) override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void HandleNetworkTest(const FInputActionValue& Value);

	void PrintNetworkState(const TCHAR* Context) const;

	/*
	 * Server RPC
	 *
	 * 소유 클라이언트에서 호출하지만,
	 * 실제 함수 본문은 서버에서 실행된다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerToggleNetworkTest();

	/*
	 * RepNotify 함수
	 *
	 * 서버의 bNetworkTestActive 값이
	 * 클라이언트에 복제되었을 때 호출된다.
	 */
	UFUNCTION()
	void OnRep_NetworkTestActive();

protected:
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Player|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Player|Equipment")
	TObjectPtr<UStaticMeshComponent> FirstPersonEquipmentMesh;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Player|Equipment")
	TObjectPtr<UStaticMeshComponent> WorldEquipmentMesh;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Input")
	TObjectPtr<UInputAction> NetworkTestAction;

	/*
	 * ReplicatedUsing
	 *
	 * 이 값은 서버가 원본을 관리한다.
	 * 값이 클라이언트에 도착하면
	 * OnRep_NetworkTestActive()가 실행된다.
	 */
	UPROPERTY(
		ReplicatedUsing = OnRep_NetworkTestActive,
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Player|Network Test")
	bool bNetworkTestActive = false;
};