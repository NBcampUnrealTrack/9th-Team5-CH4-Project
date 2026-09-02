#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "DeepRaiders/Core/Subsystem/DRVoxelTerrainSubsystem.h"
#include "AbilitySystemInterface.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "DRPlayerController.generated.h"

class ADRPlayerCharacter;
class ADRShop;
class UInputAction;
class UInputMappingContext;
class UDRInventoryComponent;
class UDRQuickSlotComponent;
class UDRStartingSelectionComponent;
class UDRShopTransactionComponent;
class UDRShopUIComponent;
class UDRItemDefinition;
class ADRWorldItemActor;
class ADRStorage;
class UDRHUDUIComponent;
class UDRSkillUIComponent;
class UDRQuickSlotUIComponent;
class UDRInventoryUIComponent;
class UDRTeleportUIComponent;
class UDRUIConfig;
class UGameplayAbility;
class UUserWidget;
class UDRScoreboardUIComponent;
class UDRStartingSelectionUIComponent;
class UDRInteractionComponent;
enum class EDRSkillSlot : uint8;
struct FGameplayAbilitySpec;
struct FPredictionKey;

// 현재 플레이어가 열고 있는 Storage에 변경이 생긴 경우
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRCurrentStorageChanged, ADRStorage*, CurrentStorage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRSnowJoinSnapshotApplied, int32, SnapshotId);

UENUM(BlueprintType)
enum class EDRStorageTransferDirection : uint8
{
	PlayerToStorage,
	StorageToPlayer
};

UENUM(BlueprintType)
enum class EDRSnowJoinLoadingPhase : uint8
{
	Idle,
	ReceivingSnapshot,
	ApplyingSnapshot,
	WaitingForControl,
	Complete
};

UCLASS()
class DEEPRAIDERS_API ADRPlayerController : public APlayerController, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADRPlayerController();
	UInputAction* GetSkillInputAction(EDRSkillSlot SkillSlot) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	virtual void Tick(float DeltaSeconds) override;

	void SetupGASInputComponent();
	bool bGASInputBound = false;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_Pawn() override;

	virtual void OnRep_PlayerState() override;

private:
	/** 현재 조종 중인 DeepRaiders 캐릭터를 반환한다. */
	ADRPlayerCharacter* GetDRPlayerCharacter() const;
	void RefreshPlayerUI();

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);

	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);

	void HandleSelectQuickSlot(const FInputActionValue& Value);

	void HandleGASInputStarted(int32 InputId);
	void HandleGASInputTriggered(int32 InputId);
	void HandleGASInputReleased(int32 InputId);
	FPredictionKey GetAbilityActivationPredictionKey(const FGameplayAbilitySpec& Spec) const;

	void InitializeStartingQuickSlot();

	// Secondary 취소 정책을 가진 활성 이동 Ability에 입력을 전달
	bool TrySendSecondaryMovementCancelEvent(int32 InputId);

	UFUNCTION()
	void RefreshPublicQuickSlotSnapshot();

	void ApplyViewPitchLimits();
	void HandleScoreboardStarted(const FInputActionValue& Value);
	void HandleScoreboardCompleted(const FInputActionValue& Value);
	void HandleToggleMenu(const FInputActionValue&);
	
private:
	/*
	 * Started 단계에서 별도 경로로 소비된 입력을 Release까지 추적
	 * Started에서 Generic 입력으로 사용한 뒤 Targeting Task가 즉시 종료되어도,
	 * 같은 입력에서 발생하는 Triggered가 일반 Ability로 전달되는 것을 방지
	 * -> 던지기 동작 중 계속 던지기를 시도하지 않도록
	 */
	TSet<int32> ConsumedStartedInputIds;

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
	TObjectPtr<UInputAction> Skill1Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> Skill2Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> InteractionAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> DropAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> ScoreboardAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> MenuAction;
	
#pragma region QuickSlot

public:
	UDRInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	UDRQuickSlotComponent* GetQuickSlotComponent() { return QuickSlotComponent; }

	/** 인벤토리와 구매한 업그레이드를 지우고 시작 장비를 다시 지급한다. */
	void ResetForGameStart();

	/** 시작 무기 선택 기능을 사용하는 UI와 ViewModel에 컴포넌트를 제공한다. */
	UDRStartingSelectionComponent* GetStartingSelectionComponent() const
	{
		return StartingSelectionComponent;
	}

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
	TObjectPtr<UDRItemDefinition> StartingRifle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|QuickSlot|Test")
	TObjectPtr<UDRItemDefinition> StartingShotgun;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|QuickSlot|Test")
	TObjectPtr<UDRItemDefinition> StartingSprayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|QuickSlot|Test")
	TObjectPtr<UDRItemDefinition> StartingCannon;

protected:

#pragma endregion

#pragma region DEBUG BUILD
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|QuickSlot|Test")
	TObjectPtr<UDRItemDefinition> TestItemDefinition1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|QuickSlot|Test")
	int32 TestItemQuantity1 = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|QuickSlot|Test")
	TObjectPtr<UDRItemDefinition> TestItemDefinition2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|QuickSlot|Test")
	int32 TestItemQuantity2 = 1;
#pragma endregion

#pragma region UI

public:
	UDRInventoryUIComponent* GetInventoryUIComponent() const
	{
		return InventoryUIComponent;
	}

	/** 상점 영역 이탈에 따른 상점 UI 종료를 처리한다. */
	void NotifyShopAreaExited(ADRShop* Shop);

	/** 현재 상점 상호작용이 가능한지 확인한다. */
	bool IsShopInteractionAvailable() const;

private:
	/** 현재 Pawn이 상호작용 영역 안에 있는 가장 가까운 상점을 찾는다. */
	ADRShop* FindInteractableShop() const;

	/** 현재 상점의 UI를 열거나 닫는다. */
	void HandleToggleShop(const FInputActionValue& Value);

protected:
	/** 로컬 플레이어 UI에서 사용할 위젯 클래스 설정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UDRUIConfig> UIConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ShopAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRShopUIComponent> ShopUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Starting Selection")
	TObjectPtr<UDRStartingSelectionComponent> StartingSelectionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRStartingSelectionUIComponent> StartingSelectionUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRHUDUIComponent> HUDUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRSkillUIComponent> SkillUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRQuickSlotUIComponent> QuickSlotUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRInventoryUIComponent> InventoryUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRTeleportUIComponent> TeleportUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|UI")
	TObjectPtr<UDRScoreboardUIComponent> ScoreboardUIComponent;

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

#pragma region Snow Join Snapshot
public:
	UPROPERTY(BlueprintAssignable, Category = "Snow|Join Snapshot")
	FDRSnowJoinSnapshotApplied OnSnowJoinSnapshotApplied;

	UFUNCTION(Client, Reliable)
	void Client_BeginSnowJoinSnapshot(
		int32 SnapshotId,
		int32 CheckpointSequence,
		FName VoxelWorldName,
		int32 VoxelSaveByteCount,
		int32 SnowVolumeByteCount,
		int32 OwnershipByteCount);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveSnowJoinSnapshotChunk(
		int32 SnapshotId,
		uint8 PayloadType,
		int32 ByteOffset,
		const TArray<uint8>& ChunkData);

	UFUNCTION(Client, Reliable)
	void Client_FinishSnowJoinSnapshot(int32 SnapshotId);

	UFUNCTION(Client, Reliable)
	void Client_ResumeSnowJoinOperations(int32 SnapshotId);

	// GameState multicast가 snapshot 적용 전에 도착하면 여기서 보관한다.
	bool QueueSnowJoinOperation(const FDRSnowOperationRecord& Record);

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestSnowJoinSnapshotData(int32 SnapshotId);

	UFUNCTION(Server, Reliable)
	void ServerNotifySnowJoinSnapshotApplied(int32 SnapshotId);

	void SendNextSnowJoinSnapshotChunk();
	void FinishSnowJoinSnapshotTransfer();
	bool TryApplyPendingSnowJoinSnapshot();
	void RetryPendingSnowJoinSnapshot();
	void ApplySnowJoinOperations(const TArray<FDRSnowOperationRecord>& Operations);

	float GetSnowJoinSnapshotProgress() const; // 현재 중도 접속 스냅샷의 네트워크 수신 진행률을 0~1 범위로 반환

	int32 OutgoingSnowSnapshotId = INDEX_NONE;
	uint8 OutgoingSnowPayloadType = 0;
	int32 OutgoingSnowByteOffset = 0;
	TArray<uint8> OutgoingSnowVoxelSaveData;
	TArray<uint8> OutgoingSnowVolumeData;
	TArray<uint8> OutgoingSnowOwnershipData;
	FTimerHandle SnowJoinSnapshotSendTimer;
	int32 ExpectedAppliedSnowSnapshotId = INDEX_NONE;
	bool bSnowSnapshotTransferFinished = false;

	int32 PendingSnowSnapshotId = INDEX_NONE;
	int32 PendingSnowCheckpointSequence = 0;
	FName PendingSnowVoxelWorldName = NAME_None;
	int32 PendingSnowVoxelSaveByteCount = 0;
	int32 PendingSnowVolumeByteCount = 0;
	int32 PendingSnowOwnershipByteCount = 0;
	bool bPendingSnowSnapshotFinished = false;
	bool bPendingSnowCheckpointApplied = false;
	EDRSnowJoinLoadingPhase SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::Idle;
	TArray<uint8> PendingSnowVoxelSaveData;
	TArray<uint8> PendingSnowVolumeData;
	TArray<uint8> PendingSnowOwnershipData;
	TArray<FDRSnowOperationRecord> BufferedSnowOperations;
	FTimerHandle SnowJoinSnapshotRetryTimer;
#pragma endregion

#pragma region Interact
public:
	UDRInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Interaction")
	TObjectPtr<UDRInteractionComponent> InteractionComponent;
#pragma endregion

#pragma region Debug
protected:
	virtual void PlayerTick(float DeltaTime) override;

private:
	void UpdateCameraAimDebug();
#pragma endregion

};
