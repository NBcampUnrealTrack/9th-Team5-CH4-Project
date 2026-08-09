#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "DRPlayerController.generated.h"

class ADRPlayerCharacter;
class UInputAction;
class UInputMappingContext;
class UDRInventoryComponent;
class UDRQuickSlotComponent;

UCLASS()
class DEEPRAIDERS_API ADRPlayerController
	: public APlayerController
{
	GENERATED_BODY()

public:
	ADRPlayerController();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	
	virtual void OnPossess(APawn* InPawn) override;

private:
	ADRPlayerCharacter* GetDRPlayerCharacter() const;

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);

	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);

	void HandleSelectQuickSlot(const FInputActionValue& Value);
	void HandleMeleeAttack(const FInputActionValue&);

	// 임시 네트워크 검증 입력
	void HandleNetworkTest(const FInputActionValue& Value);

	void HandleMine(const FInputActionValue& Value);
	
protected:
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
	TObjectPtr<UInputAction> SelectQuickSlotAction;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Input")
	TObjectPtr<UInputAction> NetworkTestAction;
	
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Input")
	TObjectPtr<UInputAction> MineAction;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Input")
	TObjectPtr<UInputAction> MeleeAttackAction;
	
#pragma region Terrain Dig
public:
	UFUNCTION(Client, Reliable)
	void Client_ApplyTerrainDigHistory(
		const TArray<FDRTerrainDigOperation>& DigHistory);

private:
	bool ApplyTerrainDigOnce(const FDRTerrainDigOperation& Operation);
#pragma endregion
	
#pragma region QuickSlot
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|QuickSlot")
	TObjectPtr<UDRInventoryComponent> QuickSlotInventoryComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|QuickSlot")
	TObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;

#pragma endregion 
};