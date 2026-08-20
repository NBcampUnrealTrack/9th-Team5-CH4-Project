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
class UDRHUDUIComponent;
class UDRQuickSlotUIComponent;
class UDRInventoryUIComponent;
class UDRTeleportUIComponent;
class UDRUIConfig;
class UGameplayAbility;
class UUserWidget;

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

private:
	/** 현재 조종 중인 DeepRaiders 캐릭터를 반환한다. */
	ADRPlayerCharacter* GetDRPlayerCharacter() const;
	
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);

	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);

	void HandleSelectQuickSlot(const FInputActionValue& Value);
	
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> InventoryAction;
	
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

#pragma region UI

public:
	UDRInventoryUIComponent* GetInventoryUIComponent() const
	{
		return InventoryUIComponent;
	}
	
	/** 상호작용 범위 안에서 입력을 받을 상점을 등록한다. */
	void SetAvailableShop(UDRShopUIComponent* ShopUIComponent);

	/** 범위를 벗어난 상점이 현재 상점이면 등록을 해제한다. */
	void ClearAvailableShop(UDRShopUIComponent* ShopUIComponent);

private:
	/** 현재 상점의 UI를 열거나 닫는다. */
	void HandleToggleShop(const FInputActionValue& Value);

protected:
	/** 로컬 플레이어 UI에서 사용할 위젯 클래스 설정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UDRUIConfig> UIConfig;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ShopAction;

	/** 로컬 플레이어가 현재 상호작용할 수 있는 상점 목록이다. */
	TArray<TWeakObjectPtr<UDRShopUIComponent>> AvailableShops;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRHUDUIComponent> HUDUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRQuickSlotUIComponent> QuickSlotUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRInventoryUIComponent> InventoryUIComponent;
	
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
