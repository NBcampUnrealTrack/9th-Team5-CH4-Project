#include "DRPlayerController.h"

#include "DRPlayerCharacter.h"
#include "Components/DRSnowJoinComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DeepRaiders/Core/Settings/DRGameUserSettings.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRItemInstance.h"
#include "DeepRaiders/Item/DRProjectileWeaponDefinition.h"
#include "DeepRaiders/Item/DRRangedWeaponDefinition.h"
#include "DeepRaiders/Item/Upgrade/DRWeaponUpgradeProfile.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Combat/Placement/DRPlacementTargetActor.h"
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
#include "DeepRaiders/UI/Loading/DRLoadingUIComponent.h"
#include "DeepRaiders/UI/Skill/DRSkillUIComponent.h"
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
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

ADRPlayerController::ADRPlayerController()
	: bCanTeleportInteract(false)
{
	SnowJoinComponent = CreateDefaultSubobject<UDRSnowJoinComponent>(TEXT("SnowJoinComponent"));

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
	LoadingUIComponent = CreateDefaultSubobject<UDRLoadingUIComponent>(TEXT("LoadingUIComponent"));
	SkillUIComponent = CreateDefaultSubobject<UDRSkillUIComponent>(TEXT("SkillUIComponent"));
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
				if (IsValid(LoadingUIComponent))
				{
					LoadingUIComponent->RefreshLoadingScreen();
				}
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
	if (IsValid(InputSubsystem) && IsValid(PlacementMappingContext))
	{
		// 이전 Pawn/Controller의 설치 입력 상태가 남아 있지 않게 기본 상태에서 제거한다.
		InputSubsystem->RemoveMappingContext(PlacementMappingContext);
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
		EnhancedInput->BindAction(MoveAction.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleMoveCompleted);
		EnhancedInput->BindAction(MoveAction.Get(), ETriggerEvent::Canceled, this, &ThisClass::HandleMoveCompleted);
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

	if (IsValid(PlacementRotateAction))
	{
		EnhancedInput->BindAction(PlacementRotateAction, ETriggerEvent::Triggered, this, &ThisClass::HandleRotatePlacement);
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
	SnowJoinComponent->LogSnowJoinControlState(TEXT("ServerPossessed"));

	ApplyViewPitchLimits();

	if (HasAuthority())
	{
		RefreshPublicQuickSlotSnapshot();
	}

	if (IsValid(QuickSlotComponent))
	{
		// PlayerState ASC의 Avatar가 바뀌므로 선택 아이템이 같아도 AbilitySet을 다시 부여한다.
		QuickSlotComponent->RefreshSelectedItem(true);
	}

	RefreshPlayerUI();
}

void ADRPlayerController::OnRep_Pawn()
{
	SnowJoinComponent->LogSnowJoinControlState(TEXT("ClientPawnReplicated"));
	Super::OnRep_Pawn();

	ApplyViewPitchLimits();
	RefreshPlayerUI();
}

void ADRPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	// RPC가 Pawn 복제보다 먼저 도착해 참조가 NULL인지 확인한다.
	UE_LOG(LogTemp, Log, TEXT("[JoinControl] ClientRestart PC=%s IncomingPawn=%s"),
		*GetNameSafe(this), *GetNameSafe(NewPawn));
	Super::ClientRestart_Implementation(NewPawn);
	SnowJoinComponent->LogSnowJoinControlState(TEXT("ClientRestartReturned"));
}

void ADRPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	SnowJoinComponent->LogSnowJoinControlState(TEXT("ClientAcknowledged"));
}

void ADRPlayerController::ServerAcknowledgePossession_Implementation(APawn* InPawn)
{
	Super::ServerAcknowledgePossession_Implementation(InPawn);
	SnowJoinComponent->LogSnowJoinControlState(TEXT("ServerReceivedAcknowledgement"));
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

void ADRPlayerController::HandleMoveCompleted(const FInputActionValue&)
{
	ADRPlayerCharacter* PlayerCharacter = GetDRPlayerCharacter();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	/*
	 * MoveAction release 시 마지막 WASD를 반드시 지운다.
	 * Zipline JumpOff에서 이전 프레임 입력을 잘못 재사용하는 것을 방지한다.
	 */
	PlayerCharacter->MoveInput(FVector2D::ZeroVector);
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

void ADRPlayerController::BeginPlacementInput(ADRPlacementTargetActor* TargetActor)
{
	if (!IsLocalController() || !IsValid(TargetActor))
	{
		return;
	}

	ActivePlacementTargetActor = TargetActor;
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = IsValid(LocalPlayer)
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer)
		: nullptr;
	if (!IsValid(InputSubsystem) || !IsValid(PlacementMappingContext))
	{
		return;
	}

	InputSubsystem->RemoveMappingContext(PlacementMappingContext);
	InputSubsystem->AddMappingContext(PlacementMappingContext, PlacementMappingPriority);
}

void ADRPlayerController::EndPlacementInput(ADRPlacementTargetActor* TargetActor)
{
	if (ActivePlacementTargetActor.Get() != TargetActor)
	{
		return;
	}

	ActivePlacementTargetActor.Reset();
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = IsValid(LocalPlayer)
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer)
		: nullptr;
	if (IsValid(InputSubsystem) && IsValid(PlacementMappingContext))
	{
		InputSubsystem->RemoveMappingContext(PlacementMappingContext);
	}
}

void ADRPlayerController::InitializeStartingQuickSlot()
{
	if (!HasAuthority() ||
		!IsValid(InventoryComponent) ||
		!IsValid(QuickSlotComponent) ||
		InventoryComponent->GetMaxSlots() < 2)
	{
		return;
	}

	if (IsValid(StartingShovelDefinition) 
		&& !InventoryComponent->GetItemAtSlot(0))
	{
		InventoryComponent->TryAddItemToSlot(0, StartingShovelDefinition, 1);
	}

	if (IsValid(StartingPistol) 
		&& !InventoryComponent->GetItemAtSlot(1))
	{
		InventoryComponent->TryAddItemToSlot(1, StartingPistol, 1);
	}
	
	if (IsValid(StartingRifle) 
		&& !InventoryComponent->GetItemAtSlot(2))
	{
		InventoryComponent->TryAddItemToSlot(2, StartingRifle, 1);
	}
	
	if (IsValid(StartingShotgun) 
		&& !InventoryComponent->GetItemAtSlot(3))
	{
		InventoryComponent->TryAddItemToSlot(3, StartingShotgun, 1);
	}
	
	if (IsValid(StartingSprayer) 
		&& !InventoryComponent->GetItemAtSlot(4))
	{
		InventoryComponent->TryAddItemToSlot(4, StartingSprayer, 1);
	}
	
	if (IsValid(StartingCannon) 
		&& !InventoryComponent->GetItemAtSlot(5))
	{
		InventoryComponent->TryAddItemToSlot(5, StartingCannon, 1);
	}

#if WITH_EDITOR

	if (!InventoryComponent->GetItemAtSlot(6))
	{
		InventoryComponent->TryAddItemToSlot(6, TestItemDefinition1, TestItemQuantity1);
	}

	if (!InventoryComponent->GetItemAtSlot(7))
	{
		InventoryComponent->TryAddItemToSlot(7, TestItemDefinition2, TestItemQuantity2);
	}

#endif

	QuickSlotComponent->RequestSelectSlot(0);
}

void ADRPlayerController::Upgrade(FString WeaponName, FString StatName)
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		ServerUpgradeWeaponForDebug(MoveTemp(WeaponName), MoveTemp(StatName));
	}
#endif
}

void ADRPlayerController::GiveWeapon(FString WeaponName)
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController())
	{
		return;
	}

	ServerGiveWeaponForDebug(MoveTemp(WeaponName));
#endif
}

void ADRPlayerController::FullSnow()
{
#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		ServerFullSnow();
	}
#endif
}

void ADRPlayerController::ServerFullSnow_Implementation()
{
#if !UE_BUILD_SHIPPING
	ADRPlayerState* PS =
		GetPlayerState<ADRPlayerState>();

	if (!IsValid(PS))
	{
		return;
	}

	const float CurrentSnow = PS->GetSnowGauge();

	// 지금 MaxSnowGauge가 1000만이라 그냥 충분히 큰 값 추가해도
	// Attribute clamp에서 MaxSnowGauge까지 잘리긴 하지만,
	// 가능하면 Max 기준으로 채우는 게 낫다.
	UAbilitySystemComponent* ASC =
		PS->GetAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		return;
	}

	const float MaxSnow = ASC->GetNumericAttribute(
		UDRPlayerAttributeSet::GetMaxSnowGaugeAttribute());

	PS->AddSnowGauge(
		FMath::Max(0.f, MaxSnow - CurrentSnow));

	ClientMessage(
		FString::Printf(
			TEXT("SnowGauge filled: %.0f"),
			PS->GetSnowGauge()));
#endif
}

void ADRPlayerController::ServerGiveWeaponForDebug_Implementation(const FString& WeaponName)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || !IsValid(InventoryComponent) || !IsValid(QuickSlotComponent))
	{
		return;
	}

	FString WeaponKey = WeaponName.ToLower();
	WeaponKey.ReplaceInline(TEXT("_"), TEXT(""));
	WeaponKey.ReplaceInline(TEXT("-"), TEXT(""));

	UDRItemDefinition* WeaponDefinition = nullptr;

	if (WeaponKey == TEXT("rifle"))
	{
		WeaponDefinition = StartingRifle.Get();
	}
	else if (WeaponKey == TEXT("shotgun") || WeaponKey == TEXT("scattergun"))
	{
		WeaponDefinition = StartingShotgun.Get();
	}
	else if (WeaponKey == TEXT("sprayer"))
	{
		WeaponDefinition = StartingSprayer.Get();
	}
	else if (WeaponKey == TEXT("cannon"))
	{
		WeaponDefinition = StartingCannon.Get();
	}

	if (!IsValid(WeaponDefinition))
	{
		ClientMessage(TEXT("Usage: giveweapon " "<rifle|shotgun|sprayer|cannon>"));

		return;
	}

	int32 EmptySlotIndex = INDEX_NONE;

	for (int32 SlotIndex = 0; SlotIndex < InventoryComponent->GetMaxSlots(); ++SlotIndex)
	{
		if (InventoryComponent->GetItemAtSlot(SlotIndex) == nullptr)
		{
			EmptySlotIndex = SlotIndex;
			break;
		}
	}

	if (EmptySlotIndex == INDEX_NONE)
	{
		ClientMessage(TEXT("GiveWeapon failed: no empty slot"));

		return;
	}

	const bool bAdded = InventoryComponent->TryAddItemToSlot(EmptySlotIndex, WeaponDefinition, 1);

	if (!bAdded)
	{
		ClientMessage(FString::Printf(TEXT("GiveWeapon failed: %s"), *WeaponName));

		return;
	}

	QuickSlotComponent->RequestSelectSlot(EmptySlotIndex);

	ClientMessage(FString::Printf(TEXT("GiveWeapon succeeded: %s -> slot %d"), *WeaponName, EmptySlotIndex + 1));
#endif
}

void ADRPlayerController::ServerUpgradeWeaponForDebug_Implementation(const FString& WeaponName, const FString& StatName)
{
#if !UE_BUILD_SHIPPING
	UDRRangedWeaponDefinition* WeaponDefinition = nullptr;
	FGameplayTag UpgradeTag;

	if (!ResolveWeaponUpgradeDebugTarget(WeaponName, StatName, WeaponDefinition, UpgradeTag))
	{
		ReportWeaponUpgradeDebugResult(FString::Printf(
			TEXT("Upgrade failed: unsupported arguments '%s %s'. Usage: upgrade <rifle|shotgun|sprayer|cannon> <stat>."),
			*WeaponName,
			*StatName));
		return;
	}

	int32 CurrentLevel = 0;

	if (!IsValid(InventoryComponent)
		|| !InventoryComponent->GetWeaponUpgradeLevel(WeaponDefinition, UpgradeTag, CurrentLevel))
	{
		ReportWeaponUpgradeDebugResult(FString::Printf(
			TEXT("Upgrade failed: %s is not in the inventory or its upgrade profile is invalid."),
			*WeaponName));
		return;
	}

	UDRWeaponUpgradeProfile* UpgradeProfile = WeaponDefinition->GetUpgradeProfile();
	const FDRWeaponUpgradeLevelData* TargetLevelData = IsValid(UpgradeProfile)
		? UpgradeProfile->FindLevelData(UpgradeTag, CurrentLevel + 1)
		: nullptr;

	if (TargetLevelData == nullptr)
	{
		ReportWeaponUpgradeDebugResult(FString::Printf(
			TEXT("Upgrade failed: %s %s is already at max level or has no next-level data."),
			*WeaponName,
			*StatName));
		return;
	}

	const FDRItemInstance* ItemInstance = InventoryComponent->FindFirstItemInstanceByDefinition(WeaponDefinition);
	if (ItemInstance == nullptr
		|| !InventoryComponent->TryUpgradeWeapon(ItemInstance->InstanceId, UpgradeTag, CurrentLevel))
	{
		ReportWeaponUpgradeDebugResult(FString::Printf(
			TEXT("Upgrade failed: server rejected %s %s at level %d."),
			*WeaponName,
			*StatName,
			CurrentLevel));
		return;
	}

	ReportWeaponUpgradeDebugResult(FString::Printf(
		TEXT("Upgrade succeeded: %s %s Lv.%d -> Lv.%d (Multiplier %.3f)."),
		*WeaponName,
		*StatName,
		CurrentLevel,
		CurrentLevel + 1,
		TargetLevelData->SetByCallerMagnitude));
#endif
}

bool ADRPlayerController::ResolveWeaponUpgradeDebugTarget(
	const FString& WeaponName,
	const FString& StatName,
	UDRRangedWeaponDefinition*& OutWeaponDefinition,
	FGameplayTag& OutUpgradeTag) const
{
	OutWeaponDefinition = nullptr;
	OutUpgradeTag = FGameplayTag();

	FString WeaponKey = WeaponName.ToLower();
	FString StatKey = StatName.ToLower();
	WeaponKey.ReplaceInline(TEXT("_"), TEXT(""));
	WeaponKey.ReplaceInline(TEXT("-"), TEXT(""));
	StatKey.ReplaceInline(TEXT("_"), TEXT(""));
	StatKey.ReplaceInline(TEXT("-"), TEXT(""));

	if (WeaponKey == TEXT("rifle"))
	{
		OutWeaponDefinition = Cast<UDRRangedWeaponDefinition>(StartingRifle.Get());

		if (StatKey == TEXT("damage"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Rifle_Damage;
		}
		else if (StatKey == TEXT("fireinterval") || StatKey == TEXT("firerate"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Rifle_FireInterval;
		}
		else if (StatKey == TEXT("snowcost"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Rifle_SnowCost;
		}
		else if (StatKey == TEXT("heat") || StatKey == TEXT("heatgeneration"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Rifle_HeatGeneration;
		}
		else if (StatKey == TEXT("absorb") || StatKey == TEXT("snowabsorb") || StatKey == TEXT("absorbpower"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Rifle_SnowAbsorbPower;
		}
		else if (StatKey == TEXT("snowadd") || StatKey == TEXT("snowamount"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Rifle_SnowAddAmount;
		}
	}
	else if (WeaponKey == TEXT("shotgun"))
	{
		OutWeaponDefinition = Cast<UDRRangedWeaponDefinition>(StartingShotgun.Get());

		if (StatKey == TEXT("damage"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Shotgun_Damage;
		}
		else if (StatKey == TEXT("fireinterval") || StatKey == TEXT("firerate"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Shotgun_FireInterval;
		}
		else if (StatKey == TEXT("snowcost"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Shotgun_SnowCost;
		}
		else if (StatKey == TEXT("projectilecount") || StatKey == TEXT("pelletcount"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Shotgun_ProjectileCount;
		}
		else if (StatKey == TEXT("heat") || StatKey == TEXT("heatgeneration"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Shotgun_HeatGeneration;
		}
		else if (StatKey == TEXT("absorb") || StatKey == TEXT("snowabsorb") || StatKey == TEXT("absorbpower"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Shotgun_SnowAbsorbPower;
		}
		else if (StatKey == TEXT("snowadd") || StatKey == TEXT("snowamount"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Shotgun_SnowAddAmount;
		}
	}
	else if (WeaponKey == TEXT("sprayer"))
	{
		OutWeaponDefinition = Cast<UDRRangedWeaponDefinition>(StartingSprayer.Get());

		if (StatKey == TEXT("damage"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Sprayer_Damage;
		}
		else if (StatKey == TEXT("freeze") || StatKey == TEXT("freezeamount") || StatKey == TEXT("freezepower"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Sprayer_FreezeAmount;
		}
		else if (StatKey == TEXT("snowcost"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Sprayer_SnowCost;
		}
		else if (StatKey == TEXT("heat") || StatKey == TEXT("heatgeneration"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Sprayer_HeatGeneration;
		}
		else if (StatKey == TEXT("absorb") || StatKey == TEXT("snowabsorb") || StatKey == TEXT("absorbpower"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Sprayer_SnowAbsorbPower;
		}
	}
	else if (WeaponKey == TEXT("cannon"))
	{
		OutWeaponDefinition = Cast<UDRRangedWeaponDefinition>(StartingCannon.Get());

		if (StatKey == TEXT("damage"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Cannon_Damage;
		}
		else if (StatKey == TEXT("fireinterval") || StatKey == TEXT("firerate"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Cannon_FireInterval;
		}
		else if (StatKey == TEXT("snowcost"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Cannon_SnowCost;
		}
		else if (StatKey == TEXT("heat") || StatKey == TEXT("heatgeneration"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Cannon_HeatGeneration;
		}
		else if (StatKey == TEXT("absorb") || StatKey == TEXT("snowabsorb") || StatKey == TEXT("absorbpower"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Cannon_SnowAbsorbPower;
		}
		else if (StatKey == TEXT("snowadd") || StatKey == TEXT("snowamount"))
		{
			OutUpgradeTag = DRGameplayTags::Weapon_Upgrade_Cannon_SnowAddAmount;
		}
	}

	return IsValid(OutWeaponDefinition) && OutUpgradeTag.IsValid();
}

void ADRPlayerController::ReportWeaponUpgradeDebugResult(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("[WeaponUpgrade][Debug] %s"), *Message);
	ClientMessage(Message);
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
	if (!HasAuthority())
	{
		return;
	}

	if (IsValid(InventoryComponent))
	{
		InventoryComponent->ResetInventory();
		InitializeStartingQuickSlot();
	}
	if (IsValid(StartingSelectionComponent))
	{
		StartingSelectionComponent->ResetSkillSelection();
	}
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
	if (ActivePlacementTargetActor.IsValid() || !IsValid(QuickSlotComponent))
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

void ADRPlayerController::HandleRotatePlacement(const FInputActionValue& Value)
{
	if (ADRPlacementTargetActor* TargetActor = ActivePlacementTargetActor.Get())
	{
		TargetActor->AddRotationInput(Value.Get<float>());
	}
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
