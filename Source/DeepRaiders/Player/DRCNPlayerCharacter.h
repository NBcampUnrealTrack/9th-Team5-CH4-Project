#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "DRCNPlayerCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;

UCLASS()
class DEEPRAIDERS_API ADRCNPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADRCNPlayerCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	// 소유 클라이언트에서 Pawn이 시작되거나 다시 빙의될 때 호출
	virtual void PawnClientRestart() override;

protected:
	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(
		UInputComponent* PlayerInputComponent) override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void PrintNetworkState(const TCHAR* Context) const;

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

	// 추가
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
};