#include "DRPlayerController.h"

#include "DRPlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DeepRaiders/Core/Interface/DRThrowableItemInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Core/Subsystem/DRWorldItemSubsystem.h"
#include "DeepRaiders/OrePooling/DROrePoolActor.h"
#include "DeepRaiders/OrePooling/DROrePoolSubsystem.h"
#include "DeepRaiders/Shop/Components/DRShopTransactionComponent.h"
#include "DeepRaiders/Shop/Components/DRShopUIComponent.h"
#include "DeepRaiders/Shop/DRShop.h"

#include "DeepRaiders/Player/Components/DRTeleportComponent.h"

#include "DeepRaiders/UI/HUD/DRHUDUIComponent.h"
#include "DeepRaiders/UI/QuickSlot/DRQuickSlotUIComponent.h"
#include "DeepRaiders/UI/Teleport/DRTeleportUIComponent.h"
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/Inventory/DRInventoryUIComponent.h"

#include "DeepRaiders/Teleport/DRTeleportPoint.h"

#include "AbilitySystemComponent.h"
#include "DRPlayerState.h"
#include "GameplayAbilitySpec.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

ADRPlayerController::ADRPlayerController()
	: bCanTeleportInteract(false)
{
	// QuickSlot Initialize
	InventoryComponent = CreateDefaultSubobject<UDRInventoryComponent>(TEXT("QuickSlotInventoryComponent"));
	QuickSlotComponent = CreateDefaultSubobject<UDRQuickSlotComponent>(TEXT("QuickSlotComponent"));
	ShopTransactionComponent = CreateDefaultSubobject<UDRShopTransactionComponent>(TEXT("ShopTransactionComponent"));
	ShopUIComponent = CreateDefaultSubobject<UDRShopUIComponent>(TEXT("ShopUIComponent"));

	// UI Component Initialize
	HUDUIComponent = CreateDefaultSubobject<UDRHUDUIComponent>(TEXT("HUDUIComponent"));
	QuickSlotUIComponent = CreateDefaultSubobject<UDRQuickSlotUIComponent>(TEXT("QuickSlotUIComponent"));
	TeleportUIComponent = CreateDefaultSubobject<UDRTeleportUIComponent>(TEXT("TeleportUIComponent"));
	InventoryUIComponent = CreateDefaultSubobject<UDRInventoryUIComponent>(TEXT("InventoryUIComponent"));
}

void ADRPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

UAbilitySystemComponent* ADRPlayerController::GetAbilitySystemComponent() const
{
	const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return nullptr;
	}

	return DRPlayerState->GetAbilitySystemComponent();
}

void ADRPlayerController::BeginPlay()
{
	// UI 컴포넌트 BeginPlay 전에 로컬 플레이어 UI 설정을 준비한다.
	if (IsLocalController())
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UDRUIManagerSubsystem* UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>())
			{
				UIManager->Configure(this, UIConfig);
			}
		}
	}

	Super::BeginPlay();

	/*
	 * 서버에서 모든 플레이어의 시작 장비를 초기화.
	 *
	 * Super::BeginPlay()가 끝난 시점에는
	 * 소유 ActorComponent들의 BeginPlay도 진행된 상태이므로
	 * QuickSlots 배열도 준비되어 있다.
	 */
	if (HasAuthority())
	{
		InitializeStartingQuickSlot();
	}

	// 입력 매핑은 이 PC에서 실제로 입력받는 컨트롤러에만 등록한다.
	if (!IsLocalController())
	{
		return;
	}

	if (IsLocalController() && IsValid(PlayerCameraManager))
	{
		PlayerCameraManager->ViewPitchMin = -55.f;
		PlayerCameraManager->ViewPitchMax = 45.f;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	if (!IsValid(LocalPlayer))
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	if (!IsValid(InputSubsystem) || !IsValid(DefaultMappingContext.Get()))
	{
		return;
	}

	UInputMappingContext* MappingContext = DefaultMappingContext.Get();

	InputSubsystem->RemoveMappingContext(MappingContext);
	InputSubsystem->AddMappingContext(MappingContext, 0);
}

void ADRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalController())
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);

	if (!IsValid(EnhancedInput))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Enhanced Input Component is invalid"), *GetName());

		return;
	}

	if (IsValid(MoveAction.Get()))
	{
		EnhancedInput->BindAction(MoveAction.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleMove);
	}

	if (IsValid(LookAction.Get()))
	{
		EnhancedInput->BindAction(LookAction.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleLook);
	}

	if (IsValid(JumpAction.Get()))
	{
		EnhancedInput->BindAction(JumpAction.Get(), ETriggerEvent::Started, this, &ThisClass::HandleJumpStarted);

		EnhancedInput->BindAction(JumpAction.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleJumpCompleted);
	}

	if (IsValid(SelectQuickSlotAction.Get()))
	{
		EnhancedInput->BindAction(SelectQuickSlotAction.Get(), ETriggerEvent::Started, this, &ThisClass::HandleSelectQuickSlot);
	}

	if (IsValid(ShopAction.Get()))
	{
		EnhancedInput->BindAction(ShopAction, ETriggerEvent::Started, this, &ThisClass::HandleToggleShop);
	}
	
	SetupGASInputComponent();
}

void ADRPlayerController::SetupGASInputComponent()
{
	if (bGASInputBound || !IsLocalController() || !IsValid(InputComponent))
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		// PlayerState가 아직 복제되지 않았다.
		// OnRep_PlayerState에서 다시 시도.
		return;
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);

	if (!IsValid(EnhancedInputComponent))
	{
		return;
	}

	if (IsValid(PrimaryAction))
	{
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Triggered, this, &ThisClass::HandleGASInputPressed, static_cast<int32>(EDRAbilityInputId::Primary));
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Completed, this, &ThisClass::HandleGASInputReleased, static_cast<int32>(EDRAbilityInputId::Primary));
	}

	if (IsValid(SecondaryAction))
	{
		EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Triggered, this, &ThisClass::HandleGASInputPressed, static_cast<int32>(EDRAbilityInputId::Secondary));
		EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Completed, this, &ThisClass::HandleGASInputReleased, static_cast<int32>(EDRAbilityInputId::Secondary));
	}
	
	if (IsValid(InventoryAction))
	{
		EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &ThisClass::HandleGASInputPressed, static_cast<int32>(EDRAbilityInputId::Inventory));
	}

	bGASInputBound = true;
}

void ADRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (IsValid(QuickSlotComponent))
	{
		QuickSlotComponent->RefreshSelectedItem();
	}

	if (IsValid(HUDUIComponent))
	{
		HUDUIComponent->RefreshPlayerCharacter();
	}
}

void ADRPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();

	if (IsValid(HUDUIComponent))
	{
		HUDUIComponent->RefreshPlayerCharacter();
	}
}

void ADRPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	SetupGASInputComponent();

	if (IsValid(HUDUIComponent))
	{
		HUDUIComponent->RefreshPerks();
	}
}

ADRPlayerCharacter* ADRPlayerController::GetDRPlayerCharacter() const
{
	return Cast<ADRPlayerCharacter>(GetPawn());
}

void ADRPlayerController::HandleMove(const FInputActionValue& Value)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->MoveInput(Value.Get<FVector2D>());
}

void ADRPlayerController::HandleLook(const FInputActionValue& Value)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->LookInput(Value.Get<FVector2D>());
}

void ADRPlayerController::HandleJumpStarted(const FInputActionValue&)
{
	if (IsMoveInputIgnored())
	{
		return;
	}

	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (IsValid(PlayerCharacter))
	{
		PlayerCharacter->HandleJumpPressed();
	}
}

void ADRPlayerController::HandleJumpCompleted(const FInputActionValue&)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (IsValid(PlayerCharacter))
	{
		PlayerCharacter->HandleJumpReleased();
	}
}

void ADRPlayerController::InitializeStartingQuickSlot()
{
	if (!HasAuthority() ||
		!IsValid(InventoryComponent) ||
		!IsValid(QuickSlotComponent) ||
		!IsValid(StartingShovelDefinition) ||
		!IsValid(StartingProjectileWeaponDefinition) ||
		InventoryComponent->GetMaxSlots() < 2)
	{
		return;
	}

	if (!InventoryComponent->GetItemAtSlot(0))
	{
		InventoryComponent->TryAddItemToSlot(0, StartingShovelDefinition, 1);
	}
	
	if (!InventoryComponent->GetItemAtSlot(1))
	{
		InventoryComponent->TryAddItemToSlot(1, StartingProjectileWeaponDefinition, 1);
	}
	
	QuickSlotComponent->RequestSelectSlot(0);	
}

void ADRPlayerController::HandleGASInputPressed(int32 InputId)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromInputID(InputId);
		if (Spec)
		{
			Spec->InputPressed = true;
			if (Spec->IsActive())
			{
				ASC->AbilitySpecInputPressed(*Spec);
			}
			else
			{
				ASC->TryActivateAbility(Spec->Handle);
			}
		}
	}
}

void ADRPlayerController::HandleGASInputReleased(int32 InputId)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromInputID(InputId);
		if (Spec)
		{
			Spec->InputPressed = false;
			if (Spec->IsActive())
			{
				ASC->AbilitySpecInputReleased(*Spec);
			}
		}
	}
}

void ADRPlayerController::HandleSelectQuickSlot(const FInputActionValue& Value)
{
	const int32 InputSlotNumber = FMath::RoundToInt(Value.Get<float>());

	// 사용자에게 보이는 1번 슬롯은 배열 인덱스 0
	const int32 SlotIndex = InputSlotNumber - 1;

	if (SlotIndex < 0)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT( "[QuickSlot Input] " "InputNumber=%d SlotIndex=%d"), InputSlotNumber, SlotIndex);

	QuickSlotComponent->RequestSelectSlot(SlotIndex);
}

void ADRPlayerController::HandleToggleShop(const FInputActionValue&)
{
	AvailableShops.RemoveAll(
		[](const TWeakObjectPtr<ADRShop>& Shop)
		{
			return !Shop.IsValid();
		});

	if (!AvailableShops.IsEmpty())
	{
		ShopUIComponent->ToggleShopWidget(AvailableShops.Last().Get());
	}
}

void ADRPlayerController::SetAvailableShop(
	ADRShop* Shop)
{
	if (!IsValid(Shop))
	{
		return;
	}

	AvailableShops.Remove(Shop);
	AvailableShops.Add(Shop);
}

void ADRPlayerController::ClearAvailableShop(
	ADRShop* Shop)
{
	AvailableShops.Remove(Shop);

	if (IsValid(ShopUIComponent))
	{
		ShopUIComponent->CloseShop(Shop);
	}
}

#pragma region Teleport
void ADRPlayerController::SetCanTeleportInteract(bool bNewCanTeleportInteract)
{
	bCanTeleportInteract = bNewCanTeleportInteract;
}

void ADRPlayerController::TryInteractCurrentTeleport()
{
	if (bCanTeleportInteract)
	{
		ServerRequestInteractCurrentTeleport();
	}
}

void ADRPlayerController::ServerRequestInteractCurrentTeleport_Implementation()
{
	APawn* CachedPawn = GetPawn();
	const UDRTeleportComponent* TeleportComponent = IsValid(CachedPawn) ? CachedPawn->FindComponentByClass<UDRTeleportComponent>() : nullptr;
	AActor* Target = IsValid(TeleportComponent) ? TeleportComponent->GetCurrentInteractableTeleport() : nullptr;

	if (!IsValid(CachedPawn) || !IsValid(Target) || !Target->Implements<UDRInteractableInterface>())
	{
		return;
	}

	if (!IDRInteractableInterface::Execute_CanInteract(Target, CachedPawn))
	{
		return;
	}

	IDRInteractableInterface::Execute_Interact(Target, CachedPawn);
}
#pragma endregion

#pragma region Snow Join Snapshot
void ADRPlayerController::Client_BeginSnowJoinSnapshot_Implementation(
	int32 SnapshotId,
	int32 CheckpointSequence,
	FName VoxelWorldName,
	int32 VoxelSaveByteCount,
	int32 SnowVolumeByteCount,
	int32 OwnershipByteCount)
{
	if (SnapshotId <= 0 || VoxelSaveByteCount <= 0 || SnowVolumeByteCount <= 0 || OwnershipByteCount <= 0)
	{
		return;
	}

	PendingSnowSnapshotId = SnapshotId;
	PendingSnowCheckpointSequence = CheckpointSequence;
	PendingSnowVoxelWorldName = VoxelWorldName;
	PendingSnowVoxelSaveByteCount = VoxelSaveByteCount;
	PendingSnowVolumeByteCount = SnowVolumeByteCount;
	PendingSnowOwnershipByteCount = OwnershipByteCount;
	bPendingSnowSnapshotFinished = false;
	PendingSnowVoxelSaveData.Reset();
	PendingSnowVolumeData.Reset();
	PendingSnowOwnershipData.Reset();
	PendingSnowHistory.Reset();
	BufferedSnowOperations.Reset();

	ServerRequestSnowJoinSnapshotData(SnapshotId);
}

void ADRPlayerController::ServerRequestSnowJoinSnapshotData_Implementation(int32 SnapshotId)
{
	UWorld* World = GetWorld();
	UDRSnowSubsystem* SnowSubsystem = IsValid(World) ? World->GetSubsystem<UDRSnowSubsystem>() : nullptr;
	ADRMiningGameStateBase* MiningGameState = IsValid(World) ? World->GetGameState<ADRMiningGameStateBase>() : nullptr;
	if (!IsValid(SnowSubsystem) || !IsValid(MiningGameState))
	{
		return;
	}

	FDRSnowJoinCheckpoint Checkpoint;
	if (!SnowSubsystem->GetCheckpoint(SnapshotId, Checkpoint))
	{
		return;
	}

	constexpr int32 ChunkByteSize = 48 * 1024;
	auto SendData = [this, SnapshotId](uint8 PayloadType, const TArray<uint8>& Data)
	{
		for (int32 Offset = 0; Offset < Data.Num(); Offset += ChunkByteSize)
		{
			const int32 Size = FMath::Min(ChunkByteSize, Data.Num() - Offset);
			TArray<uint8> ChunkData;
			ChunkData.Append(Data.GetData() + Offset, Size);
			Client_ReceiveSnowJoinSnapshotChunk(
				SnapshotId,
				PayloadType,
				Offset,
				ChunkData);
		}
	};

	SendData(0, Checkpoint.VoxelSaveData);
	SendData(1, Checkpoint.SnowVolumeData);
	SendData(2, Checkpoint.OwnershipData);

	TArray<FDRSnowOperationRecord> RecentHistory;
	MiningGameState->GetSnowOperationsAfter(Checkpoint.OperationSequence, RecentHistory);
	Client_FinishSnowJoinSnapshot(SnapshotId, RecentHistory);
}

void ADRPlayerController::Client_ReceiveSnowJoinSnapshotChunk_Implementation(
	int32 SnapshotId,
	uint8 PayloadType,
	int32 ByteOffset,
	const TArray<uint8>& ChunkData)
{
	if (SnapshotId != PendingSnowSnapshotId || ByteOffset < 0 || ChunkData.IsEmpty())
	{
		return;
	}

	TArray<uint8>* TargetData = nullptr;
	int32 ExpectedByteCount = 0;
	switch (PayloadType)
	{
	case 0:
		TargetData = &PendingSnowVoxelSaveData;
		ExpectedByteCount = PendingSnowVoxelSaveByteCount;
		break;
	case 1:
		TargetData = &PendingSnowVolumeData;
		ExpectedByteCount = PendingSnowVolumeByteCount;
		break;
	case 2:
		TargetData = &PendingSnowOwnershipData;
		ExpectedByteCount = PendingSnowOwnershipByteCount;
		break;
	default:
		return;
	}
	if (ByteOffset + ChunkData.Num() > ExpectedByteCount)
	{ 
		return;
	}

	if (TargetData->Num() < ByteOffset + ChunkData.Num())
	{
		TargetData->SetNumZeroed(ByteOffset + ChunkData.Num());
	}

	FMemory::Memcpy(TargetData->GetData() + ByteOffset, ChunkData.GetData(), ChunkData.Num());
	TryApplyPendingSnowJoinSnapshot();
}

void ADRPlayerController::Client_FinishSnowJoinSnapshot_Implementation(
	int32 SnapshotId,
	const TArray<FDRSnowOperationRecord>& RecentHistory)
{
	if (SnapshotId != PendingSnowSnapshotId)
	{
		return;
	}

	bPendingSnowSnapshotFinished = true;
	PendingSnowHistory = RecentHistory;
	TryApplyPendingSnowJoinSnapshot();
}

bool ADRPlayerController::QueueSnowJoinOperation(const FDRSnowOperationRecord& Record)
{
	if (PendingSnowSnapshotId == INDEX_NONE || Record.Sequence <= PendingSnowCheckpointSequence)
	{
		return false;
	}

	BufferedSnowOperations.Add(Record);
	return true;
}

bool ADRPlayerController::TryApplyPendingSnowJoinSnapshot()
{
	if (PendingSnowSnapshotId == INDEX_NONE || !bPendingSnowSnapshotFinished ||
		PendingSnowVoxelSaveData.Num() != PendingSnowVoxelSaveByteCount ||
		PendingSnowVolumeData.Num() != PendingSnowVolumeByteCount ||
		PendingSnowOwnershipData.Num() != PendingSnowOwnershipByteCount)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UDRSnowSubsystem* SnowSubsystem = IsValid(World) ? World->GetSubsystem<UDRSnowSubsystem>() : nullptr;
	if (!IsValid(SnowSubsystem) || !SnowSubsystem->ApplyCheckpoint(
		PendingSnowVoxelWorldName,
		PendingSnowVoxelSaveData,
		PendingSnowVolumeData,
		PendingSnowOwnershipData))
	{
		if (IsValid(World))
		{
			World->GetTimerManager().SetTimer(
				SnowJoinSnapshotRetryTimer,
				this,
				&ADRPlayerController::RetryPendingSnowJoinSnapshot,
				0.25f,
				false);
		}
		return false;
	}

	TMap<int32, FDRSnowOperationRecord> OperationsBySequence;
	for (const FDRSnowOperationRecord& Record : PendingSnowHistory)
	{
		if (Record.Sequence > PendingSnowCheckpointSequence)
		{
			OperationsBySequence.Add(Record.Sequence, Record);
		}
	}
	for (const FDRSnowOperationRecord& Record : BufferedSnowOperations)
	{
		if (Record.Sequence > PendingSnowCheckpointSequence)
		{
			OperationsBySequence.Add(Record.Sequence, Record);
		}
	}

	TArray<FDRSnowOperationRecord> Operations;
	OperationsBySequence.GenerateValueArray(Operations);
	Operations.Sort([](const FDRSnowOperationRecord& A, const FDRSnowOperationRecord& B)
	{
		return A.Sequence < B.Sequence;
	});

	const int32 AppliedSnapshotId = PendingSnowSnapshotId;
	const int32 AppliedVoxelSaveByteCount = PendingSnowVoxelSaveByteCount;
	const int32 AppliedSnowVolumeByteCount = PendingSnowVolumeByteCount;
	const int32 AppliedOwnershipByteCount = PendingSnowOwnershipByteCount;
	PendingSnowSnapshotId = INDEX_NONE;
	PendingSnowCheckpointSequence = 0;
	PendingSnowVoxelSaveData.Reset();
	PendingSnowVolumeData.Reset();
	PendingSnowOwnershipData.Reset();
	PendingSnowHistory.Reset();
	BufferedSnowOperations.Reset();
	ApplySnowJoinOperations(Operations);
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[JoinSnapshot] Applied Id=%d Voxel=%d bytes SnowVolume=%d bytes Ownership=%d bytes RecentOperations=%d"),
		AppliedSnapshotId,
		AppliedVoxelSaveByteCount,
		AppliedSnowVolumeByteCount,
		AppliedOwnershipByteCount,
		Operations.Num());
	
	return true;
}

void ADRPlayerController::RetryPendingSnowJoinSnapshot()
{
	TryApplyPendingSnowJoinSnapshot();
}

void ADRPlayerController::ApplySnowJoinOperations(const TArray<FDRSnowOperationRecord>& Operations)
{
	ADRMiningGameStateBase* MiningGameState = GetWorld() ? GetWorld()->GetGameState<ADRMiningGameStateBase>() : nullptr;
	if (!IsValid(MiningGameState))
	{
		return;
	}

	for (const FDRSnowOperationRecord& Record : Operations)
	{
		MiningGameState->ApplySnowOperationRecord(Record);
	}
}
#pragma endregion
