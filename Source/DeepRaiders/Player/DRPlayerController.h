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
class UDRItemDefinition;
class ADRWorldItemActor;

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
	/** 현재 조종 중인 DeepRaiders 캐릭터를 반환한다. */
	ADRPlayerCharacter* GetDRPlayerCharacter() const;

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);

	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);

	void HandleSelectQuickSlot(const FInputActionValue& Value);
	void HandlePrimaryAction(const FInputActionValue& value);
	void HandleSecondaryAction(const FInputActionValue& Value);

	// 임시 네트워크 검증 입력
	void HandleNetworkTest(const FInputActionValue& Value);
	
	void InitializeStartingQuickSlot();
	
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
	TObjectPtr<UInputAction> PrimaryAction;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Input")
	TObjectPtr<UInputAction> SecondaryAction;
	
#pragma region Terrain Dig
public:
	UFUNCTION(Client, Reliable)
	void Client_ApplyTerrainDigHistory(
		const TArray<FDRTerrainDigOperation>& DigHistory);

private:
	bool ApplyTerrainDigOnce(const FDRTerrainDigOperation& Operation);
#pragma endregion

#pragma region QuickSlot
public:
	UDRInventoryComponent* GetQuickSlotInventoryComponent() const { return QuickSlotInventoryComponent;}
	UDRQuickSlotComponent* GetQuickSlotComponent() {return QuickSlotComponent;}
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|QuickSlot")
	TObjectPtr<UDRInventoryComponent> QuickSlotInventoryComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|QuickSlot")
	TObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;
	
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|QuickSlot|Test")
	TObjectPtr<UDRItemDefinition> StartingShovelDefinition;
#pragma endregion 

#pragma region Interact
private:
	void HandleInteract(const FInputActionValue& Value);	
	bool TraceInteractable(FHitResult& OutHit);
	
	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(AActor* ExpectedTarget);
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> InteractAction;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Interaction", meta = (ClampMin = "0.0", UIMin ="0.0", Units = "cm"))
	float InteractionRange = 300.0f;
	
#pragma endregion
	
#pragma region Receive Item
public:
	bool CanReceiveItem(UDRItemDefinition* Definition, int32 Quantity) const;
	bool TryReceiveItem(UDRItemDefinition* Definition, int32 Quantity);
#pragma endregion
	
#pragma region Drop Item
public:
	void RequestThrowHeldItem();

private:
	void HandleDropHeldItem(const FInputActionValue& Value);
	
	// 손에 들고 있는 아이템 드랍 시도 요청
	UFUNCTION(Server, Reliable)
	void ServerRequestDropHeldItem();
	
	ADRWorldItemActor* SpawnDroppedItem(UDRItemDefinition* Definition, const FTransform& BaseSpawnTransform, int32 Quantity) const;
	void ActivateUsableDiggingItem(ADRWorldItemActor* SpawnedItem) const;
	
	// 아이템 드랍 실패 롤백
	void RollbackDroppedItem(ADRWorldItemActor* DroppedItem) const;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> DropHeldItemAction;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Drop", meta = (ClampMin = "0.0", Units = "cm"))
	float DropForwardDistance = 100.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Drop", meta = (ClampMin = "0.0", Units = "cm"))
	float DropVerticalOffset = 40.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Drop", meta = (ClampMin = "0.0"))
	float DropImpulseStrength = 300.0f;
#pragma endregion
	
};
