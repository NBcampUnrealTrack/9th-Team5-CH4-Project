#include "DRPlayerController.h"

#include "DRPlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DeepRaiders/Core/Subsystem/DRSnowSubsystem.h"
#include "DeepRaiders/Core/Settings/DRGameUserSettings.h"
#include "DeepRaiders/Core/GameModes/DRMiningGameModeBase.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Core/Subsystem/DRWorldItemSubsystem.h"
#include "DeepRaiders/OrePooling/DROrePoolActor.h"
#include "DeepRaiders/OrePooling/DROrePoolSubsystem.h"
#include "DeepRaiders/Shop/Components/DRShopTransactionComponent.h"
#include "DeepRaiders/Shop/Components/DRShopUIComponent.h"
#include "DeepRaiders/Shop/DRShop.h"

#include "DeepRaiders/Player/Components/DRTeleportComponent.h"
#include "DeepRaiders/Player/Components/DRStartingSelectionComponent.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "Components/DRInteractionComponent.h"

#include "DeepRaiders/UI/HUD/DRHUDUIComponent.h"
#include "DeepRaiders/UI/Skill/DRSkillUIComponent.h"
#include "DeepRaiders/UI/QuickSlot/DRQuickSlotUIComponent.h"
#include "DeepRaiders/UI/Teleport/DRTeleportUIComponent.h"
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"
#include "DeepRaiders/UI/Inventory/DRInventoryUIComponent.h"
#include "DeepRaiders/UI/StartingSelection/DRStartingSelectionUIComponent.h"

#include "DeepRaiders/Teleport/DRTeleportPoint.h"

#include "AbilitySystemComponent.h"
#include "DRPlayerState.h"
#include "GameplayAbilitySpec.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "GameplayPrediction.h"
#include "Abilities/GameplayAbilityTypes.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/UI/Scoreboard/DRScoreboardUIComponent.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"

#include "DrawDebugHelpers.h"
#include "DeepRaiders/Combat/Projectile/DRProjectile.h"
#include "HAL/IConsoleManager.h"

namespace DRSnowSnapshotTransfer
{
	constexpr int32 ChunkByteSize = 12 * 1024;
	constexpr float ChunkSendInterval = 0.05f;
	constexpr uint64 ProgressMessageKey = 0x4452534E;
}

ADRPlayerController::ADRPlayerController()
	: bCanTeleportInteract(false)
{
	// QuickSlot Initialize
	InventoryComponent = CreateDefaultSubobject<UDRInventoryComponent>(TEXT("QuickSlotInventoryComponent"));
	QuickSlotComponent = CreateDefaultSubobject<UDRQuickSlotComponent>(TEXT("QuickSlotComponent"));
	ShopTransactionComponent = CreateDefaultSubobject<UDRShopTransactionComponent>(TEXT("ShopTransactionComponent"));
	ShopUIComponent = CreateDefaultSubobject<UDRShopUIComponent>(TEXT("ShopUIComponent"));
	StartingSelectionComponent = CreateDefaultSubobject<UDRStartingSelectionComponent>(TEXT("StartingWeaponSelectionComponent"));
	StartingSelectionUIComponent = CreateDefaultSubobject<UDRStartingSelectionUIComponent>(TEXT("StartingSelectionUIComponent"));

	// Interaction Initialize
	InteractionComponent = CreateDefaultSubobject<UDRInteractionComponent>(TEXT("InteractionComponent"));

	// UI Component Initialize
	HUDUIComponent = CreateDefaultSubobject<UDRHUDUIComponent>(TEXT("HUDUIComponent"));
	SkillUIComponent = CreateDefaultSubobject<UDRSkillUIComponent>(TEXT("SkillUIComponent"));
	QuickSlotUIComponent = CreateDefaultSubobject<UDRQuickSlotUIComponent>(TEXT("QuickSlotUIComponent"));
	TeleportUIComponent = CreateDefaultSubobject<UDRTeleportUIComponent>(TEXT("TeleportUIComponent"));
	InventoryUIComponent = CreateDefaultSubobject<UDRInventoryUIComponent>(TEXT("InventoryUIComponent"));
	ScoreboardUIComponent = CreateDefaultSubobject<UDRScoreboardUIComponent>(TEXT("ScoreboardUIComponent"));
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

void ADRPlayerController::RequestSetPlayerName(const FString& NewPlayerName)
{
	if (!IsLocalController())
	{
		return;
	}

	const FString SanitizedName =
		UDRGameUserSettings::SanitizePlayerDisplayName(NewPlayerName);

	if (const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>())
	{
		if (DRPlayerState->GetPlayerName() == SanitizedName)
		{
			return;
		}
	}

	ServerRequestSetPlayerName(SanitizedName);
}

void ADRPlayerController::ServerRequestSetPlayerName_Implementation(
	const FString& NewPlayerName)
{
	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();

	if (!IsValid(DRPlayerState))
	{
		return;
	}

	// 클라이언트에서 정규화했더라도 서버에서 같은 규칙으로 다시 검증한다.
	const FString SanitizedName =
		UDRGameUserSettings::SanitizePlayerDisplayName(NewPlayerName);

	if (DRPlayerState->GetPlayerName() == SanitizedName)
	{
		return;
	}

	DRPlayerState->SetPlayerName(SanitizedName);
	DRPlayerState->ForceNetUpdate();
}

void ADRPlayerController::ApplySavedPlayerName()
{
	if (!IsLocalController() || !IsValid(GetPlayerState<ADRPlayerState>()))
	{
		return;
	}

	const UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get();

	if (!IsValid(UserSettings))
	{
		return;
	}

	RequestSetPlayerName(UserSettings->GetPlayerDisplayName());
}

void ADRPlayerController::RevealEnemyNameFromServer(ADRPlayerState* TargetPlayerState)
{
	if (!HasAuthority() || !IsValid(TargetPlayerState) || TargetPlayerState == GetPlayerState<ADRPlayerState>())
	{
		return;
	}

	const float SafeDuration = FMath::Max(0.f, EnemyNameRevealDuration);

	if (SafeDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ClientRevealEnemyName(TargetPlayerState, SafeDuration);
}

void ADRPlayerController::ClientRevealEnemyName_Implementation(ADRPlayerState* TargetPlayerState, float Duration)
{
	if (!IsLocalController() || !IsValid(TargetPlayerState) || Duration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	const double NewExpireTime = World->GetTimeSeconds() + Duration;

	double& StoredExpireTime = EnemyNameRevealExpireTimes.FindOrAdd(TargetPlayerState);

	/*
	 * 연속 피격 시 기존 타이머를 새로 여러 개 만들지 않고,
	 * 단일 만료 시각을 뒤로 갱신한다.
	 */
	StoredExpireTime = FMath::Max(StoredExpireTime, NewExpireTime);
}

bool ADRPlayerController::IsEnemyNameRevealActive(ADRPlayerState* TargetPlayerState) const
{
	if (!IsLocalController() || !IsValid(TargetPlayerState))
	{
		return false;
	}

	const UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	const double* ExpireTime = EnemyNameRevealExpireTimes.Find(TargetPlayerState);

	return ExpireTime != nullptr && *ExpireTime > World->GetTimeSeconds();
}

void ADRPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocalController())
	{
		return;
	}

	if (SnowJoinLoadingPhase != EDRSnowJoinLoadingPhase::Complete &&
		PendingSnowSnapshotId == INDEX_NONE &&
		GetStateName() == NAME_Playing)
	{
		const APawn* ControlledPawn = GetPawn();
		if (IsValid(ControlledPawn) && ControlledPawn->IsLocallyControlled())
		{
			SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::Complete;
			UE_LOG(LogTemp, Log, TEXT("[JoinSnapshot] Control ready Pawn=%s"), *GetNameSafe(ControlledPawn));
		}
	}

	if (GEngine == nullptr)
	{
		return;
	}

	FString LoadingStatus;
	switch (SnowJoinLoadingPhase)
	{
	case EDRSnowJoinLoadingPhase::Idle:
		LoadingStatus = TEXT("Network Sync: Idle");
		break;
	case EDRSnowJoinLoadingPhase::ReceivingSnapshot:
		LoadingStatus = FString::Printf(
			TEXT("Network Sync: Receiving Snapshot %.1f%%"),
			GetSnowJoinSnapshotProgress() * 100.f);
		break;
	case EDRSnowJoinLoadingPhase::ApplyingSnapshot:
		LoadingStatus = TEXT("Network Sync: Applying Snapshot");
		break;
	case EDRSnowJoinLoadingPhase::WaitingForControl:
		LoadingStatus = TEXT("Network Sync: Waiting For Control");
		break;
	case EDRSnowJoinLoadingPhase::Complete:
	default:
		LoadingStatus = TEXT("Network Sync: Complete");
		break;
	}

	GEngine->AddOnScreenDebugMessage(
		DRSnowSnapshotTransfer::ProgressMessageKey,
		0.1f,
		FColor::Cyan,
		LoadingStatus);
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

	// 시작 무기 선택에 필요한 기본 무기와 장비 컴포넌트를 연결한다.
	StartingSelectionComponent->Initialize(
		StartingRifle,
		InventoryComponent,
		QuickSlotComponent);

	StartingSelectionUIComponent->InitializeStartingSelection(
		StartingSelectionComponent);

	ApplyViewPitchLimits();
	ApplySavedPlayerName();

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
		QuickSlotComponent->OnQuickSlotsChangedDelegate.AddDynamic(
			this,
			&ThisClass::RefreshPublicQuickSlotSnapshot);
		RefreshPublicQuickSlotSnapshot();
	}

	// 입력 매핑은 이 PC에서 실제로 입력받는 컨트롤러에만 등록한다.
	if (!IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	if (!IsValid(LocalPlayer))
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	if (IsValid(InputSubsystem) && IsValid(DefaultMappingContext.Get()))
	{
		UInputMappingContext* MappingContext = DefaultMappingContext.Get();

		InputSubsystem->RemoveMappingContext(MappingContext);
		InputSubsystem->AddMappingContext(MappingContext, 0);
	}

	if (IsValid(SkillUIComponent) && IsValid(HUDUIComponent))
	{
		SkillUIComponent->Initialize(HUDUIComponent->GetHUDWidget());
	}
}

void ADRPlayerController::RefreshPublicQuickSlotSnapshot()
{
	if (ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>())
	{
		DRPlayerState->UpdatePublicQuickSlots(QuickSlotComponent);
	}
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

	if (IsValid(ScrollQuickSlotAction.Get()))
	{
		EnhancedInput->BindAction(ScrollQuickSlotAction.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleScrollQuickSlot);
	}

	if (IsValid(ScoreboardAction))
	{
		EnhancedInput->BindAction(ScoreboardAction, ETriggerEvent::Started, this, &ThisClass::HandleScoreboardStarted);
		EnhancedInput->BindAction(ScoreboardAction, ETriggerEvent::Completed, this, &ThisClass::HandleScoreboardCompleted);
		EnhancedInput->BindAction(ScoreboardAction, ETriggerEvent::Canceled, this, &ThisClass::HandleScoreboardCompleted);
	}

	if (IsValid(MenuAction))
	{
		EnhancedInput->BindAction(MenuAction, ETriggerEvent::Started, this, &ThisClass::HandleToggleMenu);
	}

	SetupGASInputComponent();
}

void ADRPlayerController::HandleToggleMenu(const FInputActionValue&)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer))
	{
		return;
	}

	UDRUIManagerSubsystem* UIManager = LocalPlayer->GetSubsystem<UDRUIManagerSubsystem>();
	if (!IsValid(UIManager))
	{
		return;
	}

	if (UIManager->PopTopScreen())
	{
		return;
	}

	UIManager->PushScreen(DRGameplayTags::UI_Screen_Menu);
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
		const int32 InputId = static_cast<int32>(EDRAbilityInputId::Primary);

		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Started, this, &ThisClass::HandleGASInputStarted, InputId);
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Triggered, this, &ThisClass::HandleGASInputTriggered, InputId);
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Completed, this, &ThisClass::HandleGASInputReleased, InputId);
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Canceled, this, &ThisClass::HandleGASInputReleased, InputId);
	}

	if (IsValid(SecondaryAction))
	{
		const int32 InputId = static_cast<int32>(EDRAbilityInputId::Secondary);

		EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Started, this, &ThisClass::HandleGASInputStarted, InputId);
		EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Triggered, this, &ThisClass::HandleGASInputTriggered, InputId);
		EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Completed, this, &ThisClass::HandleGASInputReleased, InputId);
		EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Canceled, this, &ThisClass::HandleGASInputReleased, InputId);
	}

	if (IsValid(Skill1Action))
	{
		EnhancedInputComponent->BindAction(Skill1Action, ETriggerEvent::Started, this, &ThisClass::HandleGASInputStarted, static_cast<int32>(EDRAbilityInputId::Skill1));
	}

	if (IsValid(Skill2Action))
	{
		EnhancedInputComponent->BindAction(Skill2Action, ETriggerEvent::Started, this, &ThisClass::HandleGASInputStarted, static_cast<int32>(EDRAbilityInputId::Skill2));
	}

	if (IsValid(InventoryAction))
	{
		EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &ThisClass::HandleGASInputStarted, static_cast<int32>(EDRAbilityInputId::Inventory));
	}

	if (IsValid(InteractionAction))
	{
		EnhancedInputComponent->BindAction(InteractionAction, ETriggerEvent::Started, this, &ThisClass::HandleGASInputStarted, static_cast<int32>(EDRAbilityInputId::Interaction));
	}

	if (IsValid(DropAction))
	{
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Started, this, &ThisClass::HandleGASInputStarted, static_cast<int32>(EDRAbilityInputId::Drop));
	}

	bGASInputBound = true;
}

void ADRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ApplyViewPitchLimits();

	if (HasAuthority())
	{
		RefreshPublicQuickSlotSnapshot();
	}

	if (IsValid(QuickSlotComponent))
	{
		QuickSlotComponent->RefreshSelectedItem();
	}

	RefreshPlayerUI();
}

void ADRPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();

	ApplyViewPitchLimits();
	RefreshPlayerUI();
}

void ADRPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	SetupGASInputComponent();
	RefreshPlayerUI();
	ApplySavedPlayerName();
}

ADRPlayerCharacter* ADRPlayerController::GetDRPlayerCharacter() const
{
	return Cast<ADRPlayerCharacter>(GetPawn());
}

void ADRPlayerController::RefreshPlayerUI()
{
	if (IsValid(HUDUIComponent))
	{
		HUDUIComponent->RefreshPlayerCharacter();
	}

	if (IsValid(SkillUIComponent))
	{
		SkillUIComponent->RefreshPlayerCharacter();
	}
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

	FVector2D LookInput = Value.Get<FVector2D>();
	if (const UDRGameUserSettings* UserSettings = UDRGameUserSettings::Get())
	{
		LookInput.X *= UserSettings->GetMouseSensitivityX();
		LookInput.Y *= UserSettings->GetMouseSensitivityY();
	}

	PlayerCharacter->LookInput(LookInput);
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
		!IsValid(StartingRifle) ||
		!IsValid(StartingShotgun) ||
		!IsValid(StartingSprayer) ||
		!IsValid(StartingCannon) ||
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
		InventoryComponent->TryAddItemToSlot(1, StartingRifle, 1);
	}

	if (!InventoryComponent->GetItemAtSlot(2))
	{
		InventoryComponent->TryAddItemToSlot(2, StartingShotgun, 1);
	}

	if (!InventoryComponent->GetItemAtSlot(3))
	{
		InventoryComponent->TryAddItemToSlot(3, StartingSprayer, 1);
	}

	if (!InventoryComponent->GetItemAtSlot(4))
	{
		InventoryComponent->TryAddItemToSlot(4, StartingCannon, 1);
	}

#if WITH_EDITOR

	if (!InventoryComponent->GetItemAtSlot(5))
	{
		InventoryComponent->TryAddItemToSlot(5, TestItemDefinition1, TestItemQuantity1);
	}

	if (!InventoryComponent->GetItemAtSlot(6))
	{
		InventoryComponent->TryAddItemToSlot(6, TestItemDefinition2, TestItemQuantity2);
	}

#endif

	QuickSlotComponent->RequestSelectSlot(0);
}

bool ADRPlayerController::TrySendSecondaryMovementCancelEvent(int32 InputId)
{
	if (InputId != static_cast<int32>(EDRAbilityInputId::Secondary))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		return false;
	}

	bool bHasCancelReceiver = false;

	// MovementAction에게 Secondary 입력을 강제로 전달
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive()
			|| Spec.Ability == nullptr)
		{
			continue;
		}

		const FGameplayTagContainer& AssetTags = Spec.Ability->GetAssetTags();

		if (AssetTags.HasTagExact(DRGameplayTags::Ability_MovementAction)
			&& AssetTags.HasTagExact(DRGameplayTags::Ability_Input_SecondaryCancel))
		{
			bHasCancelReceiver = true;
			break;
		}
	}

	if (!bHasCancelReceiver)
	{
		return false;
	}

	FGameplayEventData EventData;
	EventData.EventTag = DRGameplayTags::Event_MovementAction_Cancel;
	EventData.Instigator = GetPawn();
	EventData.Target = GetPawn();

	ASC->HandleGameplayEvent(EventData.EventTag, &EventData);

	return true;
}

void ADRPlayerController::ResetForGameStart()
{
	if (!HasAuthority() || !IsValid(InventoryComponent))
	{
		return;
	}

	InventoryComponent->ResetInventory();
	InitializeStartingQuickSlot();
}

void ADRPlayerController::ApplyViewPitchLimits()
{
	if (!IsLocalController() || !IsValid(PlayerCameraManager))
	{
		return;
	}

	const ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCameraManager->ViewPitchMin = PlayerCharacter->GetAimPitchMinDegrees();
	PlayerCameraManager->ViewPitchMax = PlayerCharacter->GetAimPitchMaxDegrees();
}

void ADRPlayerController::HandleScoreboardStarted(const FInputActionValue&)
{
	if (IsValid(ScoreboardUIComponent))
	{
		ScoreboardUIComponent->ShowScoreboard();
	}
}

void ADRPlayerController::HandleScoreboardCompleted(const FInputActionValue&)
{
	if (IsValid(ScoreboardUIComponent))
	{
		ScoreboardUIComponent->HideScoreboard();
	}
}

bool ADRPlayerController::TryToggleZiplineInteraction(int32 InputId)
{
	if (InputId
		!= static_cast<int32>(
			EDRAbilityInputId::Interaction))
	{
		return false;
	}

	ADRPlayerCharacter* PlayerCharacter =
		GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return false;
	}

	UDRMovementActionComponent* MovementAction =
		PlayerCharacter->GetMovementActionComponent();

	if (!IsValid(MovementAction)
		|| !MovementAction->IsZiplineActive())
	{
		return false;
	}

	/*
	 * 탑승 중 Interaction(E)은 새 GA_Interact를 발동하지 않고
	 * 현재 Zipline Session의 취소 요청으로 소비한다.
	 *
	 * RequestCancelZipline이 SessionId를 서버에서 검증하므로
	 * 별도 Interaction RPC를 추가하지 않는다.
	 */
	MovementAction->RequestCancelZiplineFromInteraction();

	return true;
}

void ADRPlayerController::HandleGASInputStarted(int32 InputId)
{
	if (TryToggleZiplineInteraction(InputId))
	{
		return;
	}

	if (TrySendSecondaryMovementCancelEvent(InputId))
	{
		ConsumedStartedInputIds.Add(InputId);
		return;
	}


	const bool bIsItemUseInput = InputId == static_cast<int32>(EDRAbilityInputId::Primary)
		|| InputId == static_cast<int32>(EDRAbilityInputId::Secondary);

	if (bIsItemUseInput
		&& IsValid(QuickSlotComponent)
		&& QuickSlotComponent->IsQuickSlotActivationIntervalActive())
	{
		ConsumedStartedInputIds.Add(InputId);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC))
	{
		return;
	}

	const bool bConsumedAsGenericInput = ASC->IsGenericConfirmInputBound(InputId) || ASC->IsGenericCancelInputBound(InputId);
	if (bConsumedAsGenericInput)
	{
		ConsumedStartedInputIds.Add(InputId);
	}

	ASC->AbilityLocalInputPressed(InputId);
}

void ADRPlayerController::HandleGASInputTriggered(int32 InputId)
{
	if (ConsumedStartedInputIds.Contains(InputId))
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC))
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> MatchingHandles;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.InputID == InputId && Spec.IsActive())
		{
			MatchingHandles.Add(Spec.Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : MatchingHandles)
	{
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->IsActive())
		{
			continue;
		}

		Spec->InputPressed = true;
		ASC->AbilitySpecInputPressed(*Spec);
		ASC->InvokeReplicatedEvent(
			EAbilityGenericReplicatedEvent::InputPressed,
			Spec->Handle,
			GetAbilityActivationPredictionKey(*Spec));
	}
}

void ADRPlayerController::HandleGASInputReleased(int32 InputId)
{
	if (ConsumedStartedInputIds.Remove(InputId) > 0)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (IsValid(ASC))
	{
		ASC->AbilityLocalInputReleased(InputId);
	}
}

FPredictionKey ADRPlayerController::GetAbilityActivationPredictionKey(const FGameplayAbilitySpec& Spec) const
{
	UGameplayAbility* AbilityInstance = Spec.GetPrimaryInstance();
	if (!AbilityInstance)
	{
		return FPredictionKey();
	}

	return AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey();
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

void ADRPlayerController::HandleScrollQuickSlot(const FInputActionValue& Value)
{
	if (!IsValid(QuickSlotComponent))
	{
		return;
	}

	const float WheelDelta = Value.Get<float>();

	if (FMath::IsNearlyZero(WheelDelta))
	{
		return;
	}

	// 휠 Up : 이전 슬롯, 휠 Down : 다음 슬롯
	const int32 Direction = WheelDelta > 0.f ? -1 : 1;

	QuickSlotComponent->RequestSelectAdjacentSlot(Direction);
}

void ADRPlayerController::HandleToggleShop(const FInputActionValue&)
{
	if (ADRShop* Shop = FindInteractableShop())
	{
		ShopUIComponent->ToggleShopWidget(Shop);
	}
}

ADRShop* ADRPlayerController::FindInteractableShop() const
{
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();

	if (!IsValid(ControlledPawn) || !IsValid(World))
	{
		return nullptr;
	}

	ADRShop* ClosestShop = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();

	for (TActorIterator<ADRShop> ShopIterator(World); ShopIterator; ++ShopIterator)
	{
		ADRShop* Shop = *ShopIterator;

		if (!IsValid(Shop) || !Shop->IsPawnInShopArea(ControlledPawn))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			ControlledPawn->GetActorLocation(),
			Shop->GetActorLocation());

		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestShop = Shop;
			ClosestDistanceSquared = DistanceSquared;
		}
	}

	return ClosestShop;
}

void ADRPlayerController::NotifyShopAreaExited(
	ADRShop* Shop)
{
	if (IsLocalController()
		&& IsValid(ShopUIComponent))
	{
		ShopUIComponent->CloseShop(Shop);
	}
}

bool ADRPlayerController::IsShopInteractionAvailable() const
{
	return IsValid(FindInteractableShop());
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
float ADRPlayerController::GetSnowJoinSnapshotProgress() const
{
	if (PendingSnowSnapshotId == INDEX_NONE)
	{
		return 1.f;
	}

	const int64 TotalByteCount =
		static_cast<int64>(PendingSnowVoxelSaveByteCount) +
		PendingSnowVolumeByteCount;
	if (TotalByteCount <= 0)
	{
		return 0.f;
	}

	const int64 ReceivedByteCount =
		static_cast<int64>(PendingSnowVoxelSaveData.Num()) +
		PendingSnowVolumeData.Num();
	return static_cast<float>(FMath::Clamp(
		static_cast<double>(ReceivedByteCount) / TotalByteCount,
		0.0,
		1.0));
}

void ADRPlayerController::Client_BeginSnowJoinSnapshot_Implementation(
	int32 SnapshotId,
	int32 CheckpointSequence,
	FName VoxelWorldName,
	int32 VoxelSaveByteCount,
	int32 SnowVolumeByteCount)
{
	if (SnapshotId <= 0 || VoxelSaveByteCount <= 0 || SnowVolumeByteCount <= 0)
	{
		return;
	}

	SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::ReceivingSnapshot;
	PendingSnowSnapshotId = SnapshotId;
	PendingSnowCheckpointSequence = CheckpointSequence;
	PendingSnowVoxelWorldName = VoxelWorldName;
	PendingSnowVoxelSaveByteCount = VoxelSaveByteCount;
	PendingSnowVolumeByteCount = SnowVolumeByteCount;
	bPendingSnowSnapshotFinished = false;
	PendingSnowVoxelSaveData.Reset();
	PendingSnowVolumeData.Reset();
	BufferedSnowOperations.Reset();

	ServerRequestSnowJoinSnapshotData(SnapshotId);
}

void ADRPlayerController::ServerRequestSnowJoinSnapshotData_Implementation(int32 SnapshotId)
{
	UWorld* World = GetWorld();
	UDRSnowSubsystem* SnowSubsystem = IsValid(World) ? World->GetSubsystem<UDRSnowSubsystem>() : nullptr;
	if (!IsValid(SnowSubsystem))
	{
		return;
	}

	FDRSnowJoinCheckpoint Checkpoint;
	if (!SnowSubsystem->GetCheckpoint(SnapshotId, Checkpoint))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(SnowJoinSnapshotSendTimer);
	OutgoingSnowSnapshotId = SnapshotId;
	ExpectedAppliedSnowSnapshotId = SnapshotId;
	bSnowSnapshotTransferFinished = false;
	OutgoingSnowPayloadType = 0;
	OutgoingSnowByteOffset = 0;
	OutgoingSnowVoxelSaveData = MoveTemp(Checkpoint.VoxelSaveData);
	OutgoingSnowVolumeData = MoveTemp(Checkpoint.SnowVolumeData);

	// 한 프레임에 모든 RPC를 쌓지 않고 일정 간격으로 청크 하나씩 전송한다.
	World->GetTimerManager().SetTimer(
		SnowJoinSnapshotSendTimer,
		this,
		&ADRPlayerController::SendNextSnowJoinSnapshotChunk,
		DRSnowSnapshotTransfer::ChunkSendInterval,
		true);
}

void ADRPlayerController::SendNextSnowJoinSnapshotChunk()
{
	if (OutgoingSnowSnapshotId == INDEX_NONE)
	{
		return;
	}

	// Reliable RPCs bypass the engine's normal saturation rejection. Pace the
	// snapshot explicitly so actor replication can continue on this connection.
	if (const UNetConnection* Connection = GetNetConnection();
		Connection != nullptr && !Connection->IsNetReady())
	{
		return;
	}

	const TArray<uint8>* Payload = nullptr;
	switch (OutgoingSnowPayloadType)
	{
	case 0:
		Payload = &OutgoingSnowVoxelSaveData;
		break;
	case 1:
		Payload = &OutgoingSnowVolumeData;
		break;
	default:
		FinishSnowJoinSnapshotTransfer();
		return;
	}

	if (OutgoingSnowByteOffset >= Payload->Num())
	{
		++OutgoingSnowPayloadType;
		OutgoingSnowByteOffset = 0;
		SendNextSnowJoinSnapshotChunk();
		return;
	}

	const int32 ChunkSize = FMath::Min(
		DRSnowSnapshotTransfer::ChunkByteSize,
		Payload->Num() - OutgoingSnowByteOffset);
	TArray<uint8> ChunkData;
	ChunkData.Append(Payload->GetData() + OutgoingSnowByteOffset, ChunkSize);
	Client_ReceiveSnowJoinSnapshotChunk(
		OutgoingSnowSnapshotId,
		OutgoingSnowPayloadType,
		OutgoingSnowByteOffset,
		ChunkData);
	OutgoingSnowByteOffset += ChunkSize;
}

void ADRPlayerController::FinishSnowJoinSnapshotTransfer()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(SnowJoinSnapshotSendTimer);

	Client_FinishSnowJoinSnapshot(OutgoingSnowSnapshotId);
	bSnowSnapshotTransferFinished = true;
	OutgoingSnowSnapshotId = INDEX_NONE;
	OutgoingSnowPayloadType = 0;
	OutgoingSnowByteOffset = 0;
	OutgoingSnowVoxelSaveData.Reset();
	OutgoingSnowVolumeData.Reset();
}

void ADRPlayerController::ServerNotifySnowJoinSnapshotApplied_Implementation(int32 SnapshotId)
{
	if (!bSnowSnapshotTransferFinished || SnapshotId != ExpectedAppliedSnowSnapshotId)
	{
		return;
	}

	bSnowSnapshotTransferFinished = false;
	ExpectedAppliedSnowSnapshotId = INDEX_NONE;
	if (ADRMiningGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADRMiningGameModeBase>() : nullptr)
	{
		GameMode->HandleSnowJoinSnapshotApplied(this);
	}
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

void ADRPlayerController::Client_FinishSnowJoinSnapshot_Implementation(int32 SnapshotId)
{
	if (SnapshotId != PendingSnowSnapshotId)
	{
		return;
	}

	SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::ApplyingSnapshot;
	bPendingSnowSnapshotFinished = true;
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
		PendingSnowVolumeData.Num() != PendingSnowVolumeByteCount)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UDRSnowSubsystem* SnowSubsystem = IsValid(World) ? World->GetSubsystem<UDRSnowSubsystem>() : nullptr;
	if (!IsValid(SnowSubsystem) || !SnowSubsystem->ApplyCheckpoint(
		PendingSnowVoxelWorldName,
		PendingSnowVoxelSaveData,
		PendingSnowVolumeData))
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

	if (ADRMiningGameStateBase* MiningGameState = World->GetGameState<ADRMiningGameStateBase>())
	{
		MiningGameState->ResetSnowApplicationStateForCheckpoint(PendingSnowCheckpointSequence);
	}

	SnowJoinLoadingPhase = EDRSnowJoinLoadingPhase::WaitingForControl;
	OnSnowJoinSnapshotApplied.Broadcast(PendingSnowSnapshotId);
	ServerNotifySnowJoinSnapshotApplied(PendingSnowSnapshotId);

	TMap<int32, FDRSnowOperationRecord> OperationsBySequence;
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
	PendingSnowSnapshotId = INDEX_NONE;
	PendingSnowCheckpointSequence = 0;
	PendingSnowVoxelSaveData.Reset();
	PendingSnowVolumeData.Reset();
	BufferedSnowOperations.Reset();
	ApplySnowJoinOperations(Operations);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[JoinSnapshot] Applied Id=%d Voxel=%d bytes SnowVolume=%d bytes Ownership=ServerOnly RecentOperations=%d"),
		AppliedSnapshotId,
		AppliedVoxelSaveByteCount,
		AppliedSnowVolumeByteCount,
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

UInputAction* ADRPlayerController::GetSkillInputAction(
	EDRSkillSlot SkillSlot) const
{
	switch (SkillSlot)
	{
	case EDRSkillSlot::One:
		return Skill1Action;

	case EDRSkillSlot::Two:
		return Skill2Action;

	default:
		return nullptr;
	}
}


#pragma region Debug

namespace DRPlayerControllerDebug
{
	static TAutoConsoleVariable<int32> CVarDrawCameraAim(
		TEXT("dr.Debug.CameraAim"),
		0,
		TEXT("Draw the local player's continuous camera aim trace. 0: Off, 1: On."),
		ECVF_Cheat);

	constexpr float MaxCameraAimDistance = 10000.0f;
	constexpr float ImpactRadius = 18.0f;
	constexpr float ProjectileLaunchOriginRadius = 10.0f;
}

void ADRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateCameraAimDebug();
}

void ADRPlayerController::UpdateCameraAimDebug()
{
	if (!IsLocalController()
		|| DRPlayerControllerDebug::CVarDrawCameraAim.GetValueOnGameThread() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	if (ViewDirection.IsNearlyZero())
	{
		return;
	}

	const FVector TraceEnd = ViewLocation + ViewDirection * DRPlayerControllerDebug::MaxCameraAimDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRCameraAimDebug), false);
	QueryParams.AddIgnoredActor(GetPawn());

	FHitResult HitResult;
	const bool bBlockingHit = World->LineTraceSingleByChannel(
		HitResult, ViewLocation, TraceEnd, DRCollisionChannels::Projectile, QueryParams);
	const FVector AimPoint = bBlockingHit ? HitResult.ImpactPoint : TraceEnd;
	const FColor TraceColor = bBlockingHit ? FColor::Green : FColor::Cyan;

	// 한 프레임만 그리되 매 Tick 갱신하여, 카메라 중심 레이가 항상 보이도록 한다.
	DrawDebugLine(World, ViewLocation, AimPoint, TraceColor, false, 0.0f, 0, 2.0f);
	DrawDebugSphere(World, AimPoint, DRPlayerControllerDebug::ImpactRadius, 12,
		TraceColor, false, 0.0f, 0, 1.5f);
	DrawDebugDirectionalArrow(World, ViewLocation, ViewLocation + ViewDirection * 120.0f,
		24.0f, FColor::White, false, 0.0f, 0, 2.0f);
	DrawDebugString(World, AimPoint + FVector(0.0f, 0.0f, 30.0f),
		bBlockingHit ? TEXT("Camera Aim: HIT") : TEXT("Camera Aim: NO HIT"), nullptr,
		TraceColor, 0.0f, false, 1.0f);

	const FDRItemInstance* SelectedItem = IsValid(InventoryComponent) && IsValid(QuickSlotComponent)
		? InventoryComponent->FindItemInstance(QuickSlotComponent->GetSelectedInstanceId())
		: nullptr;
	const UDRProjectileWeaponItemDefinition* ProjectileWeapon = SelectedItem != nullptr
		? Cast<UDRProjectileWeaponItemDefinition>(SelectedItem->Definition.Get())
		: nullptr;
	ADRPlayerCharacter* DebugCharacter = GetDRPlayerCharacter();

	if (!IsValid(ProjectileWeapon)
		|| !ProjectileWeapon->ProjectileClass
		|| !IsValid(DebugCharacter))
	{
		return;
	}

	// 실제 눈총과 같은 원거리 무기 공통 StartOffset에서 디버그 궤적을 시작한다.
	const FVector LaunchOrigin = DebugCharacter->GetActorLocation()
		+ DebugCharacter->GetActorTransform().TransformVectorNoScale(
			ProjectileWeapon->StartOffset);

	const FVector LaunchDirection = ProjectileWeapon->ResolveCameraAimDirection(
		ViewDirection,
		LaunchOrigin,
		AimPoint);

	const FVector LaunchTraceEnd = LaunchOrigin + LaunchDirection *
		FMath::Max(ProjectileWeapon->MaxAttackDistance, 1.0f);
	FCollisionQueryParams LaunchQueryParams(SCENE_QUERY_STAT(DRProjectileLaunchDebug), false);
	LaunchQueryParams.AddIgnoredActor(GetPawn());

	FHitResult LaunchHit;
	const bool bLaunchBlockingHit = World->LineTraceSingleByChannel(
		LaunchHit, LaunchOrigin, LaunchTraceEnd, DRCollisionChannels::Projectile, LaunchQueryParams);
	const FVector LaunchImpactPoint = bLaunchBlockingHit ? LaunchHit.ImpactPoint : LaunchTraceEnd;
	const FColor LaunchTraceColor = bLaunchBlockingHit ? FColor::Yellow : FColor::Orange;

	// 투사체가 실제로 사용하는 Launch 벡터를 기준으로 한 진단용 Trace다.
	DrawDebugSphere(World, LaunchOrigin, DRPlayerControllerDebug::ProjectileLaunchOriginRadius, 12,
		LaunchTraceColor, false, 0.0f, 0, 1.5f);
	DrawDebugLine(World, LaunchOrigin, LaunchImpactPoint, LaunchTraceColor, false, 0.0f, 0, 2.0f);
	DrawDebugDirectionalArrow(World, LaunchOrigin, LaunchOrigin + LaunchDirection * 120.0f,
		24.0f, LaunchTraceColor, false, 0.0f, 0, 2.0f);
	DrawDebugString(World, LaunchOrigin + FVector(0.0f, 0.0f, 25.0f),
		bLaunchBlockingHit ? TEXT("Projectile Launch Trace: HIT") : TEXT("Projectile Launch Trace: NO HIT"),
		nullptr, LaunchTraceColor, 0.0f, false, 0.9f);

	const FDRProjectileWeaponSnowAbsorbSettings& AbsorbSettings = ProjectileWeapon->SnowAbsorbSettings;
	if (!AbsorbSettings.bEnabled
		|| AbsorbSettings.Range <= 0.0f
		|| AbsorbSettings.Radius <= 0.0f)
	{
		return;
	}

	FVector AbsorbDirection = ViewDirection;
	const FVector AbsorbOrigin = DebugCharacter->GetActorLocation();
	const FVector AbsorbStart = AbsorbOrigin + DebugCharacter->GetActorTransform().TransformVectorNoScale(
		ProjectileWeapon->StartOffset);
	const float CameraAimDistance = FVector::Distance(AbsorbStart, AimPoint);
	AbsorbDirection = ProjectileWeapon->ResolveCameraAimDirection(
		ViewDirection,
		AbsorbStart,
		AimPoint);
	const bool bUseAbsorbAimCorrection = ProjectileWeapon->AimCorrectionSettings.bUseCameraAimCorrection
		&& CameraAimDistance >= ProjectileWeapon->AimCorrectionSettings.MinCameraAimCorrectionDistance;

	// StartOffset으로 고정한 시작점을 기준으로 보정된 방향의 프러스텀을 구성한다.
	const FVector AbsorbEnd = AbsorbStart + AbsorbDirection * AbsorbSettings.Range;
	const float AbsorbStartRadius = AbsorbSettings.Radius *
		FMath::Clamp(AbsorbSettings.InnerRadiusRatio, 0.0f, 1.0f);

	FVector AxisY;
	FVector AxisZ;
	AbsorbDirection.FindBestAxisVectors(AxisY, AxisZ);

	const FColor AbsorbColor(190, 80, 255);
	DrawDebugLine(World, AbsorbStart, AbsorbEnd, AbsorbColor, false, 0.0f, 0, 2.0f);
	DrawDebugCircle(World, AbsorbStart, AbsorbStartRadius, 24, FColor::Green, false,
		0.0f, 0, 2.0f, AxisY, AxisZ, false);
	DrawDebugCircle(World, AbsorbEnd, AbsorbSettings.Radius, 24, AbsorbColor, false,
		0.0f, 0, 2.0f, AxisY, AxisZ, false);
	DrawDebugLine(World, AbsorbStart + AxisY * AbsorbStartRadius,
		AbsorbEnd + AxisY * AbsorbSettings.Radius, AbsorbColor, false, 0.0f, 0, 1.5f);
	DrawDebugLine(World, AbsorbStart - AxisY * AbsorbStartRadius,
		AbsorbEnd - AxisY * AbsorbSettings.Radius, AbsorbColor, false, 0.0f, 0, 1.5f);
	DrawDebugLine(World, AbsorbStart + AxisZ * AbsorbStartRadius,
		AbsorbEnd + AxisZ * AbsorbSettings.Radius, AbsorbColor, false, 0.0f, 0, 1.5f);
	DrawDebugLine(World, AbsorbStart - AxisZ * AbsorbStartRadius,
		AbsorbEnd - AxisZ * AbsorbSettings.Radius, AbsorbColor, false, 0.0f, 0, 1.5f);
	const FString AbsorbDebugText = FString::Printf(
		TEXT("Snow Absorb: %s (Aim %.0f cm / Min %.0f cm)"),
		bUseAbsorbAimCorrection ? TEXT("Corrected") : TEXT("Forward"),
		CameraAimDistance,
		ProjectileWeapon->AimCorrectionSettings.MinCameraAimCorrectionDistance);
	DrawDebugString(World, AbsorbEnd + FVector(0.0f, 0.0f, 25.0f), AbsorbDebugText,
		nullptr, AbsorbColor, 0.0f, false, 0.9f);

}

#pragma endregion
