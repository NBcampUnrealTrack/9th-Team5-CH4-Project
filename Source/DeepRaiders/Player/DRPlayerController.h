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
class UDRShopTransactionComponent;
class UDRItemDefinition;
class ADRWorldItemActor;
class ADRStorage;
class UDRInventoryUIComponent;
class UDRQuickSlotUIComponent;

// 현재 플레이어가 열고 있는 Storage에 변경이 생긴 경우
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRCurrentStorageChanged, ADRStorage*, CurrentStorage);

UENUM(BlueprintType)
enum class EDRStorageTransferDirection : uint8
{	
	PlayerToStorage,
	StorageToPlayer
};

UCLASS()
class DEEPRAIDERS_API ADRPlayerController
	: public APlayerController
{
	GENERATED_BODY()

public:
	ADRPlayerController();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
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
	void HandlePrimaryActionStarted(const FInputActionValue& Value);
	void HandlePrimaryActionTriggered(const FInputActionValue& Value);
	void HandlePrimaryActionCompleted(const FInputActionValue& Value);
	void HandleSecondaryActionStarted(const FInputActionValue& Value);
	void HandleSecondaryActionTriggered(const FInputActionValue& Value);
	void HandleSecondaryActionCompleted(const FInputActionValue& Value);

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
	UDRInventoryComponent* GetInventoryComponent() const { return InventoryComponent;}
	UDRQuickSlotComponent* GetQuickSlotComponent() {return QuickSlotComponent;}
	UDRShopTransactionComponent* GetShopTransactionComponent() const
	{
		return ShopTransactionComponent;
	}
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|QuickSlot")
	TObjectPtr<UDRInventoryComponent> InventoryComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|QuickSlot")
	TObjectPtr<UDRQuickSlotComponent> QuickSlotComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Shop")
	TObjectPtr<UDRShopTransactionComponent> ShopTransactionComponent;
	
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
private:
	void HandleDropHeldItem(const FInputActionValue& Value);
	
	// 손에 들고 있는 아이템 드랍 시도 요청
	UFUNCTION(Server, Reliable)
	void ServerRequestDropHeldItem();
	
	ADRWorldItemActor* ConsumeAndSpawnHeldItem(const FTransform& BaseSpawnTransform, int32 Quantity) const;
	
	ADRWorldItemActor* SpawnDroppedItem(UDRItemDefinition* Definition, const FTransform& BaseSpawnTransform, int32 Quantity) const;
	
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
	
#pragma region Throw Item
	
public:
	void RequestThrowHeldItem();
	
private:
	UFUNCTION(Server, Reliable)
	void ServerRequestThrowHeldItem();
	
	bool BuildThrowAim(FTransform& OutSpawnTransform, FVector& OutThrowDirection) const;
	void NotifyThrownItem(ADRWorldItemActor* ThrownItem, APawn* Thrower) const;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Throw")
	float ThrowForwardDistance = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Throw")
	float ThrowRightOffset = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Throw")
	float ThrowVerticalOffset = -15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Throw")
	float ThrowImpulseStrength = 600.f;

#pragma endregion 
	
#pragma region Interact Storage
public:
	// 서버에서 검증된 Storage 접근 시도
	bool TryOpenStorage(ADRStorage* Storage);
	
	// 현재 Storage와 플레이어 인벤토리 사이의 아이템 이동을 요청
	UFUNCTION(BlueprintCallable, Category = "Player|Storage")
	void RequestTransferStorageItem(EDRStorageTransferDirection Direction, FGuid SourceEntryId);
	
	// 현재 Storage 접근 종료
	UFUNCTION(BlueprintCallable, Category = "Player|Storage")
	void RequestCloseStorage();
	
	UFUNCTION(BlueprintPure, Category = "Player|Storage")
	ADRStorage* GetCurrentStorage() const
	{
		return CurrentStorage.Get();
	}
	
	// CurrentStorage와 상호작용한 거리인지 검사
	UFUNCTION(BlueprintPure, Category = "Player|Storage")
	bool IsStorageWithinInteractionRange(const ADRStorage* Storage) const;
	
	// 테스트 명령
	UFUNCTION(Exec)
	void DRDepositFirstItem();
	
	UFUNCTION(Exec)
	void DRWithDrawFirstItem();
	
private:
	UFUNCTION(Server, Reliable)
	void ServerRequestTransferStorageItem(EDRStorageTransferDirection Direction, FGuid SourceEntryId);
	
	UFUNCTION(Server, Reliable)
	void ServerRequestCloseStorage();
	
	UFUNCTION()
	void OnRep_CurrentStorage();
	
	bool TryTransferStorageItemInternal(EDRStorageTransferDirection Direction, FGuid SourceEntryId);
	bool CanAccessStorage(ADRStorage* Storage) const;
	void SetCurrentStorage(ADRStorage* NewStorage);
	
public:
	UPROPERTY(BlueprintAssignable, Category = "Player|Storage")
	FDRCurrentStorageChanged OnCurrentStorageChangedDelegate;
	
protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentStorage, VisibleInstanceOnly, Category = "Player|Storage")
	TObjectPtr<ADRStorage> CurrentStorage;
	
#pragma endregion
	
#pragma region UI
private:
	void HandleToggleInventory(const FInputActionValue& Value);
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> InventoryAction;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|UI")
	TObjectPtr<UDRInventoryUIComponent> InventoryUIComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|UI")
	TObjectPtr<UDRQuickSlotUIComponent> QuickSlotUIComponent;

#pragma endregion 
};
