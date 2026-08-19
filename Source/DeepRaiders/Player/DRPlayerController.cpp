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
#include "DeepRaiders/Shop/Components/DRShopUIComponent.h"

#include "DeepRaiders/Player/Components/DRTeleportComponent.h"

#include "DeepRaiders/UI/HUD/DRHUDUIComponent.h"
#include "DeepRaiders/UI/QuickSlot/DRQuickSlotUIComponent.h"
#include "DeepRaiders/UI/Teleport/DRTeleportUIComponent.h"
#include "DeepRaiders/UI/Core/DRUIConfig.h"
#include "DeepRaiders/UI/Core/DRUIManagerSubsystem.h"

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
	HUDUIComponent = CreateDefaultSubobject<UDRHUDUIComponent>(TEXT("HUDUIComponent"));
	QuickSlotUIComponent = CreateDefaultSubobject<UDRQuickSlotUIComponent>(TEXT("QuickSlotUIComponent"));
	TeleportUIComponent = CreateDefaultSubobject<UDRTeleportUIComponent>(TEXT("TeleportUIComponent"));
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
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Triggered, this, &ThisClass::HandleGASInputPressed, static_cast<int32>(EDRAbilityInputID::Primary));
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Completed, this, &ThisClass::HandleGASInputReleased, static_cast<int32>(EDRAbilityInputID::Primary));
	}

	if (IsValid(SecondaryAction))
	{
		EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Triggered, this, &ThisClass::HandleGASInputPressed, static_cast<int32>(EDRAbilityInputID::Secondary));
		EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Completed, this, &ThisClass::HandleGASInputReleased, static_cast<int32>(EDRAbilityInputID::Secondary));
	}

	bGASInputBound = true;
}

void ADRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (IsValid(QuickSlotComponent))
	{
		QuickSlotComponent->ApplySelectedItemToCharacter();
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
		!IsValid(StartingProjectileWeaponDefinition))
	{
		return;
	}

	// 1번 = 삽, 2번 = 눈총이 필요
	if (QuickSlotComponent->GetSlotCount() < 2)
	{
		UE_LOG(LogTemp, Error, TEXT( "[StartingItem] " "At least 2 quick slots are required. " "Controller=%s"), *GetName());

		return;
	}

	// ===== 1. 시작 삽 지급 =====

	if (InventoryComponent->GetItemCount(StartingShovelDefinition) <= 0)
	{
		InventoryComponent->TryAddItem(StartingShovelDefinition, 1);
	}

	// ===== 2. 시작 눈총 지급 =====

	if (InventoryComponent->GetItemCount(StartingProjectileWeaponDefinition) <= 0)
	{
		InventoryComponent->TryAddItem(StartingProjectileWeaponDefinition, 1);
	}

	// ===== 3. 퀵슬롯 고정 배치 =====

	// 사용자 기준 1번 슬롯 = Index 0 = 삽
	if (!QuickSlotComponent->IsSlotBound(0))
	{
		QuickSlotComponent->RequestBindSlot(0, StartingShovelDefinition);
	}

	// 사용자 기준 2번 슬롯 = Index 1 = 눈총
	if (!QuickSlotComponent->IsSlotBound(1))
	{
		QuickSlotComponent->RequestBindSlot(1, StartingProjectileWeaponDefinition);
	}

	// ===== 4. 기본 장비는 삽 =====

	if (QuickSlotComponent->GetSelectedSlotIndex() == INDEX_NONE)
	{
		QuickSlotComponent->RequestSelectSlot(0);
	}
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
	if (IsValid(AvailableShop))
	{
		AvailableShop->ToggleShopWidget();
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
