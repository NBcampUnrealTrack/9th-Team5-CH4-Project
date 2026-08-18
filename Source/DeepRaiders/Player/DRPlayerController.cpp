#include "DRPlayerController.h"

#include "DRPlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DeepRaiders/Core/Interface/DRThrowableItemInterface.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Core/Subsystem/DRWorldItemSubsystem.h"
#include "DeepRaiders/OrePooling/DROrePoolActor.h"
#include "DeepRaiders/OrePooling/DROrePoolSubsystem.h"
#include "DeepRaiders/Shop/Components/DRShopTransactionComponent.h"

#include "DeepRaiders/Storage/DRStorage.h"
#include "DeepRaiders/Player/Components/DRTeleportComponent.h"

#include "DeepRaiders/UI/Inventory/DRInventoryUIComponent.h"
#include "DeepRaiders/UI/QuickSlot/DRQuickSlotUIComponent.h"
#include "DeepRaiders/UI/Teleport/DRTeleportUIComponent.h"

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

	// UI Component Initialize
	InventoryUIComponent = CreateDefaultSubobject<UDRInventoryUIComponent>(TEXT("InventoryUIComponent"));
	QuickSlotUIComponent = CreateDefaultSubobject<UDRQuickSlotUIComponent>(TEXT("QuickSlotUIComponent"));
	TeleportUIComponent = CreateDefaultSubobject<UDRTeleportUIComponent>(TEXT("TeleportUIComponent"));
}

void ADRPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CurrentStorage);
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

	if (IsValid(PrimaryAction.Get()))
	{
		EnhancedInput->BindAction(PrimaryAction.Get(), ETriggerEvent::Started, this, &ThisClass::HandlePrimaryActionStarted);

		EnhancedInput->BindAction(PrimaryAction.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandlePrimaryActionTriggered);

		EnhancedInput->BindAction(PrimaryAction.Get(), ETriggerEvent::Completed, this, &ThisClass::HandlePrimaryActionCompleted);
	}

	if (IsValid(SecondaryAction.Get()))
	{
		EnhancedInput->BindAction(SecondaryAction.Get(), ETriggerEvent::Started, this, &ThisClass::HandleSecondaryActionStarted);

		EnhancedInput->BindAction(SecondaryAction.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleSecondaryActionTriggered);

		EnhancedInput->BindAction(SecondaryAction.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleSecondaryActionCompleted);
	}

	if (IsValid(InteractAction.Get()))
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::HandleInteract);
	}

	if (IsValid(DropHeldItemAction.Get()))
	{
		EnhancedInput->BindAction(DropHeldItemAction, ETriggerEvent::Started, this, &ThisClass::HandleDropHeldItem);
	}

	if (IsValid(InventoryAction.Get()))
	{
		EnhancedInput->BindAction(InventoryAction, ETriggerEvent::Started, this, &ThisClass::HandleToggleInventory);
	}
	
	SetupGASInputComponent();
}

void ADRPlayerController::SetupGASInputComponent()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (IsValid(InputComponent))
		{
			UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);
			
			EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Triggered
				, this, &ThisClass::HandleGASInputPressed, static_cast<int32>(EDRAbilityInputID::Primary));
			EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Completed
				, this, &ThisClass::HandleGASInputReleased, static_cast<int32>(EDRAbilityInputID::Primary));
			EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Triggered
				, this, &ThisClass::HandleGASInputPressed, static_cast<int32>(EDRAbilityInputID::Secondary));
			EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Completed
				, this, &ThisClass::HandleGASInputReleased, static_cast<int32>(EDRAbilityInputID::Secondary));
		}
	}
}

void ADRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (IsValid(QuickSlotComponent))
	{
		QuickSlotComponent->ApplySelectedItemToCharacter();
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
	if (!HasAuthority() || !IsValid(InventoryComponent) || !IsValid(QuickSlotComponent) || !IsValid(StartingShovelDefinition))
	{
		return;
	}

	// QuickSlotComponent::BeginPlay가 정상적으로
	// 완료됐는지 방어적으로 확인
	if (QuickSlotComponent->GetSlotCount() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT( "[StartingItem] QuickSlot is not initialized. " "Controller=%s"), *GetName());

		return;
	}

	// 1. 인벤토리에 시작 삽 지급
	if (InventoryComponent->GetItemCount(StartingShovelDefinition) <= 0)
	{
		const bool bAdded = InventoryComponent->TryAddItem(StartingShovelDefinition, 1);

		UE_LOG(LogTemp, Warning, TEXT( "[StartingItem] Shovel Add=%d"), bAdded);
	}

	if (QuickSlotComponent->TryBindFirstEmptySlot(StartingShovelDefinition))
	{
		UE_LOG(LogTemp, Warning, TEXT( "[StartingItem] Success Bind"));
	}

	// 아무 슬롯도 선택되지 않았다면 1번 선택
	if (QuickSlotComponent->GetSelectedSlotIndex() == INDEX_NONE)
	{
		QuickSlotComponent->RequestSelectSlot(0);
	}
}

void ADRPlayerController::HandlePrimaryActionStarted(const FInputActionValue&)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->RequestPrimaryItemAction(EDRItemActionTriggerEvent::Started);
}

void ADRPlayerController::HandlePrimaryActionTriggered(const FInputActionValue&)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->RequestPrimaryItemAction(EDRItemActionTriggerEvent::Triggered);
}

void ADRPlayerController::HandlePrimaryActionCompleted(const FInputActionValue&)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->RequestPrimaryItemAction(EDRItemActionTriggerEvent::Completed);
}

void ADRPlayerController::HandleSecondaryActionStarted(const FInputActionValue&)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->RequestSecondaryItemAction(EDRItemActionTriggerEvent::Started);
}

void ADRPlayerController::HandleSecondaryActionTriggered(const FInputActionValue&)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->RequestSecondaryItemAction(EDRItemActionTriggerEvent::Triggered);
}

void ADRPlayerController::HandleSecondaryActionCompleted(const FInputActionValue&)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->RequestSecondaryItemAction(EDRItemActionTriggerEvent::Completed);
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

#pragma region Terrain Dig
void ADRPlayerController::Client_ApplyTerrainDigHistory_Implementation(const TArray<FDRTerrainDigOperation>& DigHistory)
{
	// PostLogin 이후 받은 서버 지형 이력은 순서대로 TerrainSubsystem에 위임한다.
	for (const FDRTerrainDigOperation& Operation : DigHistory)
	{
		ApplyTerrainDigOnce(Operation);
	}
}

bool ADRPlayerController::ApplyTerrainDigOnce(const FDRTerrainDigOperation& Operation)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	UDRVoxelTerrainSubsystem* TerrainSubsystem = World->GetSubsystem<UDRVoxelTerrainSubsystem>();
	if (!IsValid(TerrainSubsystem))
	{
		return false;
	}

	// VoxelWorld가 아직 생성되지 않았다면 Subsystem이 delegate 기반 pending으로 보관한다.
	return TerrainSubsystem->ApplyOrQueueDig(Operation);
}
#pragma endregion

void ADRPlayerController::HandleInteract(const FInputActionValue&)
{
	UE_LOG(LogTemp, Log, TEXT("Interact Called"));

	FHitResult Hit;

	// 상호작용 가능한 액터 탐색
	if (!IsLocalController())
	{
		return;
	}

	if (!TraceInteractable(Hit))
	{
		TryInteractCurrentTeleport();
		return;
	}

	// Interface 구현 여부 확인
	AActor* Target = Hit.GetActor();
	if (!IsValid(Target) || !Target->Implements<UDRInteractableInterface>())
	{
		return;
	}

	// 클라에서 Trace된 액터 전달
	ServerRequestInteract(Target);
}

bool ADRPlayerController::TraceInteractable(FHitResult& OutHit)
{
	const APawn* CachedPawn = GetPawn();
	UWorld* World = GetWorld();

	if (!IsValid(CachedPawn) || !IsValid(World))
	{
		return false;
	}

	const FVector Start = CachedPawn->GetPawnViewLocation();
	const FVector End = Start + CachedPawn->GetBaseAimRotation().Vector() * InteractionRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteracterTrace));
	Params.AddIgnoredActor(CachedPawn);

	return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}

void ADRPlayerController::ServerRequestInteract_Implementation(AActor* ExpectedTarget)
{
	APawn* CachedPawn = GetPawn();
	FHitResult ServerHit;

	// ExpectedTarget = 클라이언트에서 전달한 상호작용 액터
	// 서버에서 유효한 동작인지 검증
	if (!IsValid(CachedPawn) || !IsValid(ExpectedTarget) || !ExpectedTarget->Implements<UDRInteractableInterface>() || !TraceInteractable(ServerHit) || ServerHit.GetActor() != ExpectedTarget)
	{
		return;
	}

	if (!IDRInteractableInterface::Execute_CanInteract(ExpectedTarget, CachedPawn))
	{
		return;
	}

	IDRInteractableInterface::Execute_Interact(ExpectedTarget, CachedPawn);
}

bool ADRPlayerController::CanReceiveItem(UDRItemDefinition* Definition, int32 Quantity) const
{
	return HasAuthority() && IsValid(InventoryComponent) && InventoryComponent->CanAddItem(Definition, Quantity);
}

bool ADRPlayerController::TryReceiveItem(UDRItemDefinition* Definition, int32 Quantity)
{
	// 퀵슬롯 여부와는 상관없이 아이템은 추가될 수 있다.
	if (!CanReceiveItem(Definition, Quantity) || !InventoryComponent->TryAddItem(Definition, Quantity))
	{
		return false;
	}

	if (IsValid(QuickSlotComponent))
	{
		QuickSlotComponent->TryBindFirstEmptySlot(Definition);
	}

	return true;
}

void ADRPlayerController::HandleDropHeldItem(const FInputActionValue& Value)
{
	if (IsLocalController())
	{
		ServerRequestDropHeldItem();
	}
}

void ADRPlayerController::ServerRequestDropHeldItem_Implementation()
{
	APawn* CachedPawn = GetPawn();
	if (!IsValid(CachedPawn))
	{
		return;
	}

	const FVector DropDirection = CachedPawn->GetActorForwardVector();
	const FVector SpawnItemLocation = CachedPawn->GetActorLocation() + DropDirection * DropForwardDistance + FVector::UpVector * DropVerticalOffset;
	const FRotator SpawnRotation(0.f, CachedPawn->GetActorRotation().Yaw, 0.f);
	ADRWorldItemActor* DroppedItem = ConsumeAndSpawnHeldItem(FTransform(SpawnRotation, SpawnItemLocation), 1);

	if (IsValid(DroppedItem) && !FMath::IsNearlyZero(DropImpulseStrength))
	{
		DroppedItem->ApplyDropImpulse(DropDirection * DropImpulseStrength);
	}
}

ADRWorldItemActor* ADRPlayerController::ConsumeAndSpawnHeldItem(const FTransform& BaseSpawnTransform, int32 Quantity) const
{
	APawn* CachedPawn = GetPawn();

	if (!HasAuthority() || !IsValid(CachedPawn) || !IsValid(QuickSlotComponent) || !IsValid(InventoryComponent) || Quantity <= 0)
	{
		return nullptr;
	}

	UDRItemDefinition* Definition = QuickSlotComponent->GetSelectedItemDefinition();

	if (!IsValid(Definition) || InventoryComponent->GetItemCount(Definition) < Quantity)
	{
		return nullptr;
	}

	ADRWorldItemActor* SpawnedItem = SpawnDroppedItem(Definition, BaseSpawnTransform, Quantity);

	if (!IsValid(SpawnedItem))
	{
		return nullptr;
	}

	if (!InventoryComponent->TryRemoveItemByDefinition(Definition, Quantity))
	{
		RollbackDroppedItem(SpawnedItem);
		return nullptr;
	}

	return SpawnedItem;
}

ADRWorldItemActor* ADRPlayerController::SpawnDroppedItem(UDRItemDefinition* Definition, const FTransform& BaseSpawnTransform, int32 Quantity) const
{
	if (!HasAuthority() || !IsValid(Definition))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return nullptr;
	}

	if (Definition->Category == EItemCategory::Ore)
	{
		UClass* ActorClass = Definition->ActorClass.Get();

		if (!IsValid(ActorClass) || !ActorClass->IsChildOf(ADROrePoolActor::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("[%s] Invalid ore ActorClass: %s"), *GetName(), *GetNameSafe(ActorClass));

			return nullptr;
		}

		TSubclassOf<ADROrePoolActor> OreActorClass = ActorClass;

		UDROrePoolSubsystem* OrePoolSubsystem = World->GetSubsystem<UDROrePoolSubsystem>();
		if (!IsValid(OrePoolSubsystem))
		{
			return nullptr;
		}

		return OrePoolSubsystem->AcquireOre(Definition, OreActorClass, BaseSpawnTransform, INDEX_NONE);
	}

	UDRWorldItemSubsystem* WorldItemSubsystem = World->GetSubsystem<UDRWorldItemSubsystem>();
	if (!IsValid(WorldItemSubsystem))
	{
		return nullptr;
	}

	return WorldItemSubsystem->SpawnWorldItemFromDefinition(Definition, BaseSpawnTransform, Quantity);
}

void ADRPlayerController::RollbackDroppedItem(ADRWorldItemActor* DroppedItem) const
{
	if (!IsValid(DroppedItem))
	{
		return;
	}

	ADROrePoolActor* DroppedOre = Cast<ADROrePoolActor>(DroppedItem);
	if (IsValid(DroppedOre))
	{
		UWorld* World = DroppedOre->GetWorld();

		if (!IsValid(World))
		{
			return;
		}

		UDROrePoolSubsystem* OrePoolSubsystem = World->GetSubsystem<UDROrePoolSubsystem>();

		if (!IsValid(OrePoolSubsystem))
		{
			UE_LOG(LogTemp, Error, TEXT("[%s] OrePoolSubsystem is invalid."), *GetName());

			DroppedOre->Destroy();
			return;
		}

		OrePoolSubsystem->ReleaseOre(DroppedOre);
	}

	DroppedItem->Destroy();
}

void ADRPlayerController::RequestThrowHeldItem()
{
	if (IsLocalController())
	{
		ServerRequestThrowHeldItem();
	}
}

void ADRPlayerController::ServerRequestThrowHeldItem_Implementation()
{
	FTransform SpawnTransform;
	FVector ThrowDirection;

	if (!BuildThrowAim(SpawnTransform, ThrowDirection))
	{
		return;
	}

	APawn* CachedPawn = GetPawn();
	ADRWorldItemActor* ThrownItem = ConsumeAndSpawnHeldItem(SpawnTransform, 1);

	if (IsValid(ThrownItem))
	{
		ThrownItem->MarkAsThrown(CachedPawn);
		NotifyThrownItem(ThrownItem, CachedPawn);

		if (!FMath::IsNearlyZero(ThrowImpulseStrength))
		{
			ThrownItem->ApplyDropImpulse(ThrowDirection * ThrowImpulseStrength);
		}
	}
}

bool ADRPlayerController::BuildThrowAim(FTransform& OutSpawnTransform, FVector& OutThrowDirection) const
{
	const APawn* CachedPawn = GetPawn();
	if (!IsValid(CachedPawn))
	{
		return false;
	}

	const FRotator ViewRotation = CachedPawn->GetBaseAimRotation();
	const FRotationMatrix ViewRotationMatrix(ViewRotation);
	const FVector ViewForward = ViewRotation.Vector();
	const FVector ViewRight = ViewRotationMatrix.GetUnitAxis(EAxis::Y);
	const FVector ViewUp = ViewRotationMatrix.GetUnitAxis(EAxis::Z);
	const FVector SpawnItemLocation = CachedPawn->GetPawnViewLocation() + ViewForward * ThrowForwardDistance + ViewRight * ThrowRightOffset + ViewUp * ThrowVerticalOffset;

	OutSpawnTransform = FTransform(ViewRotation, SpawnItemLocation);
	OutThrowDirection = ViewForward;
	return true;
}

void ADRPlayerController::NotifyThrownItem(ADRWorldItemActor* ThrownItem, APawn* Thrower) const
{
	if (!HasAuthority() || !IsValid(ThrownItem) || !ThrownItem->Implements<UDRThrowableItemInterface>())
	{
		return;
	}

	IDRThrowableItemInterface::Execute_NotifyThrown(ThrownItem, Thrower);
}

bool ADRPlayerController::TryOpenStorage(ADRStorage* Storage)
{
	if (!HasAuthority() || !CanAccessStorage(Storage))
	{
		return false;
	}

	APawn* ControlledPawn = GetPawn();

	if (!IsValid(ControlledPawn))
	{
		return false;
	}

	if (!Storage->TryClaimOwnership(ControlledPawn))
	{
		return false;
	}

	SetCurrentStorage(Storage);
	return true;
}

void ADRPlayerController::RequestTransferStorageItem(EDRStorageTransferDirection Direction, FGuid SourceEntryId)
{
	if (!SourceEntryId.IsValid())
	{
		return;
	}

	if (HasAuthority())
	{
		TryTransferStorageItemInternal(Direction, SourceEntryId);
		return;
	}

	if (IsLocalController())
	{
		ServerRequestTransferStorageItem(Direction, SourceEntryId);
	}
}

void ADRPlayerController::ServerRequestTransferStorageItem_Implementation(EDRStorageTransferDirection Direction, FGuid SourceEntryId)
{
	TryTransferStorageItemInternal(Direction, SourceEntryId);
}

bool ADRPlayerController::TryTransferStorageItemInternal(EDRStorageTransferDirection Direction, FGuid SourceEntryId)
{
	ADRStorage* Storage = CurrentStorage.Get();

	if (!CanAccessStorage(Storage))
	{
		SetCurrentStorage(nullptr);
		return false;
	}

	UDRInventoryComponent* StorageInventory = Storage->GetInventoryComponent();

	if (!IsValid(InventoryComponent) || !IsValid(StorageInventory))
	{
		return false;
	}

	UDRInventoryComponent* SourceInventory = nullptr;
	UDRInventoryComponent* DestinationInventory = nullptr;

	switch (Direction)
	{
	case EDRStorageTransferDirection::PlayerToStorage:
		SourceInventory = InventoryComponent;
		DestinationInventory = StorageInventory;
		break;

	case EDRStorageTransferDirection::StorageToPlayer:
		SourceInventory = StorageInventory;
		DestinationInventory = InventoryComponent;
		break;

	default:
		return false;
	}

	constexpr int32 TransferQuantity = 1;

	return SourceInventory->TryTransferFromEntry(DestinationInventory, SourceEntryId, TransferQuantity) == TransferQuantity;
}

bool ADRPlayerController::CanAccessStorage(ADRStorage* Storage) const
{
	APawn* ControlledPawn = GetPawn();

	if (!HasAuthority() || !IsValid(ControlledPawn) || !IsStorageWithinInteractionRange(Storage))
	{
		return false;
	}

	return IDRInteractableInterface::Execute_CanInteract(Storage, ControlledPawn);
}

void ADRPlayerController::RequestCloseStorage()
{
	if (HasAuthority())
	{
		SetCurrentStorage(nullptr);
		return;
	}

	if (IsLocalController())
	{
		ServerRequestCloseStorage();
	}
}

void ADRPlayerController::ServerRequestCloseStorage_Implementation()
{
	SetCurrentStorage(nullptr);
}

void ADRPlayerController::SetCurrentStorage(ADRStorage* NewStorage)
{
	if (!HasAuthority() || CurrentStorage == NewStorage)
	{
		return;
	}

	CurrentStorage = NewStorage;
	OnCurrentStorageChangedDelegate.Broadcast(CurrentStorage.Get());
	ForceNetUpdate();
}

void ADRPlayerController::HandleToggleInventory(const FInputActionValue&)
{
	if (IsValid(InventoryUIComponent))
	{
		InventoryUIComponent->TogglePlayerInventory();
	}
}

void ADRPlayerController::SetAvailableShop(
	UDRShopUIComponent* ShopUIComponent)
{
	AvailableShop = ShopUIComponent;
}

void ADRPlayerController::ClearAvailableShop(
	UDRShopUIComponent* ShopUIComponent)
{
	if (AvailableShop == ShopUIComponent)
	{
		AvailableShop = nullptr;
	}
}

void ADRPlayerController::OnRep_CurrentStorage()
{
	OnCurrentStorageChangedDelegate.Broadcast(CurrentStorage.Get());
}

bool ADRPlayerController::IsStorageWithinInteractionRange(const ADRStorage* Storage) const
{
	const APawn* ControlledPawn = GetPawn();

	if (!IsValid(ControlledPawn) || !IsValid(Storage))
	{
		return false;
	}

	return FVector::DistSquared(ControlledPawn->GetActorLocation(), Storage->GetActorLocation()) <= FMath::Square(InteractionRange);
}

void ADRPlayerController::DRDepositFirstItem()
{
	if (!IsValid(InventoryComponent))
	{
		return;
	}

	const TArray<FDRInventoryEntry> Entries = InventoryComponent->GetEntries();

	if (!Entries.IsEmpty())
	{
		RequestTransferStorageItem(EDRStorageTransferDirection::PlayerToStorage, Entries[0].EntryId);
	}
}

void ADRPlayerController::DRWithDrawFirstItem()
{
	ADRStorage* Storage = CurrentStorage.Get();

	if (!IsValid(Storage) || !IsValid(Storage->GetInventoryComponent()))
	{
		return;
	}

	const TArray<FDRInventoryEntry> Entries = Storage->GetInventoryComponent()->GetEntries();

	if (!Entries.IsEmpty())
	{
		RequestTransferStorageItem(EDRStorageTransferDirection::StorageToPlayer, Entries[0].EntryId);
	}
}

void ADRPlayerController::DRTestAddSnow()
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GAS][TestActivate] Character invalid"));

		return;
	}

	UAbilitySystemComponent* ASC = PlayerCharacter->GetAbilitySystemComponent();

	if (!IsValid(ASC) || !IsValid(TestAddSnowAbilityClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GAS][TestActivate] ASC or AbilityClass invalid"));

		return;
	}

	FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromClass(TestAddSnowAbilityClass);

	UE_LOG(LogTemp, Warning, TEXT( "[GAS][TestActivate] " "NetMode=%s " "LocalController=%d " "SpecFound=%d " "Ability=%s"), *ToString(GetNetMode()), IsLocalController(), AbilitySpec != nullptr, *GetNameSafe(TestAddSnowAbilityClass));

	if (AbilitySpec == nullptr)
	{
		return;
	}

	const bool bRequested = ASC->TryActivateAbility(AbilitySpec->Handle, true);

	UE_LOG(LogTemp, Warning, TEXT( "[GAS][TestActivate] " "TryActivateAbility=%d"), bRequested);
}

void ADRPlayerController::DRTestFrozen()
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	UAbilitySystemComponent* ASC = PlayerCharacter->GetAbilitySystemComponent();

	if (!IsValid(ASC) || !IsValid(TestFrozenAbilityClass))
	{
		return;
	}

	ASC->TryActivateAbilityByClass(TestFrozenAbilityClass, true);
}

void ADRPlayerController::DRCheckFrozen()
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	UAbilitySystemComponent* ASC = IsValid(PlayerCharacter) ? PlayerCharacter->GetAbilitySystemComponent() : nullptr;

	if (!IsValid(ASC))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GAS][Frozen][ClientCheck] Frozen=%d"), ASC->HasMatchingGameplayTag( DRGameplayTags::State_Frozen));
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
