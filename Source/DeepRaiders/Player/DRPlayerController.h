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
class UDRProjectileWeaponItemDefinition;
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
struct FGameplayTag;
class ADRPlayerState;

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

	/**
	 * 서버에서 유효한 적 피격이 확정됐을 때 호출한다.
	 * 실제 표시 상태는 소유 클라이언트에서만 저장한다.
	 */
	void RevealEnemyNameFromServer(ADRPlayerState* TargetPlayerState);

	/** 해당 적의 이름 노출 시간이 아직 남아 있는지 확인한다. */
	bool IsEnemyNameRevealActive(ADRPlayerState* TargetPlayerState) const;

	/** 로컬 설정의 플레이어 이름을 현재 서버 세션에 반영한다. */
	void RequestSetPlayerName(const FString& NewPlayerName);
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	virtual void Tick(float DeltaSeconds) override;

	void SetupGASInputComponent();
	bool bGASInputBound = false;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_Pawn() override;
	virtual void ClientRestart_Implementation(APawn* NewPawn) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;
	virtual void ServerAcknowledgePossession_Implementation(APawn* InPawn) override;

	virtual void OnRep_PlayerState() override;

private:
	/** 현재 조종 중인 DeepRaiders 캐릭터를 반환한다. */
	ADRPlayerCharacter* GetDRPlayerCharacter() const;
	void RefreshPlayerUI();

	void HandleMove(const FInputActionValue& Value);
	void HandleMoveCompleted(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);

	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);

	void HandleSelectQuickSlot(const FInputActionValue& Value);
	
	void HandleScrollQuickSlot(const FInputActionValue& Value);	
	
	// Interaction 입력은 Zipline 탑승 중이면 새 상호작용 대신 현재 Zipline 해제로 사용한다.
	bool TryToggleZiplineInteraction(int32 InputId);

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

	/** PlayerState가 준비된 뒤 저장된 로컬 이름을 서버에 제출한다. */
	void ApplySavedPlayerName();

	UFUNCTION(Server, Reliable)
	void ServerRequestSetPlayerName(const FString& NewPlayerName);
	
	UFUNCTION(Client, Reliable)
	void ClientRevealEnemyName(ADRPlayerState* TargetPlayerState, float Duration);

	/** 적을 마지막으로 맞힌 시점부터 이름을 유지할 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Nameplate", meta = (ClampMin = "0.0", Units = "s"))
	float EnemyNameRevealDuration = 3.f;

	/*
	 * 관찰자별 로컬 상태다.
	 * Target PlayerState마다 서버가 허용한 노출 만료 시각을 보관한다.
	 */
	TMap<TWeakObjectPtr<ADRPlayerState>, double> EnemyNameRevealExpireTimes;
	
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
	TObjectPtr<UInputAction> ScrollQuickSlotAction;
	
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

	/** 인벤토리를 지우고 시작 장비를 다시 지급한다. */
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
	TObjectPtr<UDRItemDefinition> StartingPistol;
	
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

public:
	/** 콘솔에서 upgrade rifle damage 형태로 무기 스탯을 한 단계 올린다. */
	UFUNCTION(Exec)
	void Upgrade(FString WeaponName, FString StatName);

	UFUNCTION(Exec)
	void GiveWeapon(FString WeaponName);
	
private:
	UFUNCTION(Server, Reliable)
	void ServerUpgradeWeaponForDebug(const FString& WeaponName, const FString& StatName);

	bool ResolveWeaponUpgradeDebugTarget(
		const FString& WeaponName,
		const FString& StatName,
		UDRProjectileWeaponItemDefinition*& OutWeaponDefinition,
		FGameplayTag& OutUpgradeTag) const;

	void ReportWeaponUpgradeDebugResult(const FString& Message);

	UFUNCTION(Server, Reliable)
	void ServerGiveWeaponForDebug(const FString& WeaponName);
	
protected:
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
		int32 SnowVolumeByteCount);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveSnowJoinSnapshotChunk(
		int32 SnapshotId,
		uint8 PayloadType,
		int32 ByteOffset,
		const TArray<uint8>& ChunkData);

	UFUNCTION(Client, Reliable)
	void Client_FinishSnowJoinSnapshot(int32 SnapshotId);

	// GameState multicast가 snapshot 적용 전에 도착하면 여기서 보관한다.
	bool QueueSnowJoinOperation(const FDRSnowOperationRecord& Record);

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestSnowJoinSnapshotData(int32 SnapshotId);

	UFUNCTION(Server, Reliable)
	void ServerAckSnowJoinSnapshotChunk(
		int32 SnapshotId,
		uint8 PayloadType,
		int32 ByteOffset);

	UFUNCTION(Server, Reliable)
	void ServerNotifySnowJoinSnapshotApplied(int32 SnapshotId);

	void SendNextSnowJoinSnapshotChunk();
	void AdjustSnowSnapshotWindow(bool bIncrease, const TCHAR* Reason);
	void FinishSnowJoinSnapshotTransfer();
	bool TryApplyPendingSnowJoinSnapshot();
	void RetryPendingSnowJoinSnapshot();
	void LogSnowJoinControlState(const TCHAR* Stage) const;
	void ApplySnowJoinOperations(const TArray<FDRSnowOperationRecord>& Operations);

	float GetSnowJoinSnapshotProgress() const; // 현재 중도 접속 스냅샷의 네트워크 수신 진행률을 0~1 범위로 반환

	int32 OutgoingSnowSnapshotId = INDEX_NONE;
	uint8 OutgoingSnowPayloadType = 0;
	int32 OutgoingSnowByteOffset = 0;
	TArray<uint8> OutgoingSnowVoxelSaveData;
	TArray<uint8> OutgoingSnowVolumeData;
	// 전송 시각으로 ACK 왕복 시간을 측정하고 동시 전송 수를 2~16개로 조절한다.
	TMap<uint64, double> PendingSnowChunkAcks;
	int32 SnowSnapshotWindow = 8;
	int32 FastSnowSnapshotAcks = 0;
	double SnowSnapshotSaturationStart = -1.0;
	double LastSnowSnapshotWindowChange = 0.0;
	FTimerHandle SnowSnapshotSendTimer;
	int32 ExpectedAppliedSnowSnapshotId = INDEX_NONE;
	bool bSnowSnapshotTransferFinished = false;

	int32 PendingSnowSnapshotId = INDEX_NONE;
	int32 PendingSnowCheckpointSequence = 0;
	FName PendingSnowVoxelWorldName = NAME_None;
	int32 PendingSnowVoxelSaveByteCount = 0;
	int32 PendingSnowVolumeByteCount = 0;
	bool bPendingSnowSnapshotFinished = false;
	EDRSnowJoinLoadingPhase SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::Idle;
	TArray<uint8> PendingSnowVoxelSaveData;
	TArray<uint8> PendingSnowVolumeData;
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
