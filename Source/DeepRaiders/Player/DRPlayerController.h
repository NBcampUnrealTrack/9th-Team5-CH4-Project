#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "AbilitySystemInterface.h"
#include "DRPlayerController.generated.h"

class ADRPlayerCharacter;
class UInputAction;
class UInputMappingContext;
class UDRInventoryComponent;
class UDRQuickSlotComponent;
class UDRShopTransactionComponent;
class UDRShopUIComponent;
class UDRItemDefinition;
class ADRWorldItemActor;
class ADRStorage;
class UDRInventoryUIComponent;
class UDRHUDUIComponent;
class UDRQuickSlotUIComponent;
class UDRTeleportUIComponent;
class UDRUIConfig;
class UGameplayAbility;
class UUserWidget;
class UDRPlayerHUDWidget;

// 현재 플레이어가 열고 있는 Storage에 변경이 생긴 경우
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRCurrentStorageChanged, ADRStorage*, CurrentStorage);

UENUM(BlueprintType)
enum class EDRStorageTransferDirection : uint8
{
	PlayerToStorage,
	StorageToPlayer
};

UCLASS()
class DEEPRAIDERS_API ADRPlayerController : public APlayerController, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADRPlayerController();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	
	void SetupGASInputComponent();
	bool bGASInputBound = false;
	
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_Pawn() override;

	virtual void OnRep_PlayerState() override;

	void InitializePlayerHUD();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|UI")
	TSubclassOf<UDRPlayerHUDWidget> PlayerHUDWidgetClass;
	
private:
	/** 현재 조종 중인 DeepRaiders 캐릭터를 반환한다. */
	ADRPlayerCharacter* GetDRPlayerCharacter() const;

	UPROPERTY(Transient)
	TObjectPtr<UDRPlayerHUDWidget> PlayerHUDWidget;
	
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
	
	void HandleGASInputPressed(int32 InputId);
	void HandleGASInputReleased(int32 InputId);

	void InitializeStartingQuickSlot();
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> SelectQuickSlotAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> PrimaryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> SecondaryAction;
	
#pragma region Terrain Dig

public:
	UFUNCTION(Client, Reliable)
	void Client_ApplyTerrainDigHistory(const TArray<FDRTerrainDigOperation>& DigHistory);

private:
	bool ApplyTerrainDigOnce(const FDRTerrainDigOperation& Operation);
#pragma endregion

#pragma region QuickSlot

public:
	UDRInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	UDRQuickSlotComponent* GetQuickSlotComponent() { return QuickSlotComponent; }

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|QuickSlot|Test")
	TObjectPtr<UDRItemDefinition> StartingShovelDefinition;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|QuickSlot|Test")
	TObjectPtr<UDRItemDefinition> StartingProjectileWeaponDefinition;
	
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

	float GetStorageDistanceCheckInterval() const { return StorageDistanceCheckInterval; }

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

	/** 열린 창고와의 거리를 다시 검사하는 주기다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Storage",
		meta = (ClampMin = "0.05", Units = "s"))
	float StorageDistanceCheckInterval = 0.2f;

#pragma endregion

#pragma region UI

public:
	/** 상호작용 범위 안에서 입력을 받을 상점을 등록한다. */
	void SetAvailableShop(UDRShopUIComponent* ShopUIComponent);

	/** 범위를 벗어난 상점이 현재 상점이면 등록을 해제한다. */
	void ClearAvailableShop(UDRShopUIComponent* ShopUIComponent);

private:
	void HandleToggleInventory(const FInputActionValue& Value);

	/** 현재 상점의 UI를 열거나 닫는다. */
	void HandleToggleShop(const FInputActionValue& Value);

protected:
	/** 로컬 플레이어 UI에서 사용할 위젯 클래스 설정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UDRUIConfig> UIConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ShopAction;

	/** 로컬 플레이어가 현재 상호작용할 수 있는 상점이다. */
	UPROPERTY(Transient)
	TObjectPtr<UDRShopUIComponent> AvailableShop;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRInventoryUIComponent> InventoryUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRHUDUIComponent> HUDUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRQuickSlotUIComponent> QuickSlotUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRTeleportUIComponent> TeleportUIComponent;

#pragma endregion

#pragma region Teleport

public:
	void SetCanTeleportInteract(bool bNewCanTeleportInteract);
	bool CanTeleportInteract() const { return bCanTeleportInteract; }

private:
	void TryInteractCurrentTeleport();

	UFUNCTION(Server, Reliable)
	void ServerRequestInteractCurrentTeleport();

	uint8 bCanTeleportInteract : 1;
#pragma endregion
};
