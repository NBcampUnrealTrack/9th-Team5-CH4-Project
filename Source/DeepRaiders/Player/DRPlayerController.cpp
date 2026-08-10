#include "DRPlayerController.h"

#include "DRPlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "Components/DRQuickSlotComponent.h"

#include "DeepRaiders/Item/DRItemDefinition.h"

ADRPlayerController::ADRPlayerController()
{
    // QuickSlot Initialize
    QuickSlotInventoryComponent = CreateDefaultSubobject<UDRInventoryComponent>(TEXT("QuickSlotInventoryComponent"));
    QuickSlotComponent = CreateDefaultSubobject<UDRQuickSlotComponent>(TEXT("QuickSlotComponent"));
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

    ULocalPlayer* LocalPlayer = GetLocalPlayer();

    if (!IsValid(LocalPlayer))
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        ULocalPlayer::GetSubsystem<
            UEnhancedInputLocalPlayerSubsystem>(
                LocalPlayer);

    if (!IsValid(InputSubsystem) ||
        !IsValid(DefaultMappingContext.Get()))
    {
        return;
    }

    UInputMappingContext* MappingContext =
        DefaultMappingContext.Get();

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

    UEnhancedInputComponent* EnhancedInput =
        Cast<UEnhancedInputComponent>(InputComponent);

    if (!IsValid(EnhancedInput))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[%s] Enhanced Input Component is invalid"),
            *GetName());

        return;
    }

    if (IsValid(MoveAction.Get()))
    {
        EnhancedInput->BindAction(
            MoveAction.Get(),
            ETriggerEvent::Triggered,
            this,
            &ThisClass::HandleMove);
    }

    if (IsValid(LookAction.Get()))
    {
        EnhancedInput->BindAction(
            LookAction.Get(),
            ETriggerEvent::Triggered,
            this,
            &ThisClass::HandleLook);
    }

    if (IsValid(JumpAction.Get()))
    {
        EnhancedInput->BindAction(
            JumpAction.Get(),
            ETriggerEvent::Started,
            this,
            &ThisClass::HandleJumpStarted);

        EnhancedInput->BindAction(
            JumpAction.Get(),
            ETriggerEvent::Completed,
            this,
            &ThisClass::HandleJumpCompleted);
    }

    if (IsValid(SelectQuickSlotAction.Get()))
    {
        EnhancedInput->BindAction(
            SelectQuickSlotAction.Get(),
            ETriggerEvent::Started,
            this,
            &ThisClass::HandleSelectQuickSlot);
    }

    if (IsValid(NetworkTestAction.Get()))
    {
        EnhancedInput->BindAction(
            NetworkTestAction.Get(),
            ETriggerEvent::Started,
            this,
            &ThisClass::HandleNetworkTest);
    }
    
    if (IsValid(PrimaryAction.Get()))
    {
        EnhancedInput->BindAction(
            PrimaryAction.Get(),
            ETriggerEvent::Started,
            this,
            &ThisClass::HandlePrimaryAction);
    }
    
    if (IsValid(SecondaryAction.Get()))
    {
        EnhancedInput->BindAction(
            SecondaryAction,
            ETriggerEvent::Started,
            this,
            &ThisClass::HandleSecondaryAction);
    }
}

void ADRPlayerController::OnPossess(
    APawn* InPawn)
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

void ADRPlayerController::HandleMove(
    const FInputActionValue& Value)
{
    ADRPlayerCharacter* PlayerCharacter =
        GetDRPlayerCharacter();

    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    PlayerCharacter->MoveInput(
        Value.Get<FVector2D>());
}

void ADRPlayerController::HandleLook(
    const FInputActionValue& Value)
{
    ADRPlayerCharacter* PlayerCharacter =
        GetDRPlayerCharacter();

    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    PlayerCharacter->LookInput(
        Value.Get<FVector2D>());
}

void ADRPlayerController::HandleJumpStarted(
    const FInputActionValue&)
{
    ADRPlayerCharacter* PlayerCharacter =
        GetDRPlayerCharacter();

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->HandleJumpPressed();
    }
}

void ADRPlayerController::HandleJumpCompleted(
    const FInputActionValue&)
{
    ADRPlayerCharacter* PlayerCharacter =
        GetDRPlayerCharacter();

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->HandleJumpReleased();
    }
}

void ADRPlayerController::HandleNetworkTest(
    const FInputActionValue& Value)
{
    ADRPlayerCharacter* PlayerCharacter =
        GetDRPlayerCharacter();

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->RequestNetworkTest();
    }
}

void ADRPlayerController::InitializeStartingQuickSlot()
{
    if (!HasAuthority() ||
        !IsValid(QuickSlotInventoryComponent) ||
        !IsValid(QuickSlotComponent) ||
        !IsValid(StartingShovelDefinition))
    {
        return;
    }

    // QuickSlotComponent::BeginPlay가 정상적으로
    // 완료됐는지 방어적으로 확인
    if (QuickSlotComponent->GetSlotCount() <= 0)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "[StartingItem] QuickSlot is not initialized. "
                "Controller=%s"),
            *GetName());

        return;
    }

    // 1. 인벤토리에 시작 삽 지급
    if (QuickSlotInventoryComponent->GetItemCount(
            StartingShovelDefinition) <= 0)
    {
        const bool bAdded =
            QuickSlotInventoryComponent->TryAddItem(
                StartingShovelDefinition,
                1);

        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "[StartingItem] Shovel Add=%d"),
            bAdded);
    }

    // 2. 1번 퀵슬롯(배열 0번)에 삽 바인딩
    if (!QuickSlotComponent->IsSlotBound(0))
    {
        QuickSlotComponent->RequestBindSlot(
            0,
            StartingShovelDefinition);
    }

    // 3. 아무 슬롯도 선택되지 않았다면 1번 선택
    if (QuickSlotComponent->GetSelectedSlotIndex()
        == INDEX_NONE)
    {
        QuickSlotComponent->RequestSelectSlot(0);
    }
}

void ADRPlayerController::HandlePrimaryAction(const FInputActionValue& value)
{
    ADRPlayerCharacter* PlayerCharacter =
        Cast<ADRPlayerCharacter>(GetPawn());

    if (!IsValid(PlayerCharacter))
    {
        return;
    }
    
    PlayerCharacter->RequestPrimaryItemAction();
}

void ADRPlayerController::HandleSecondaryAction(
    const FInputActionValue& Value)
{
    ADRPlayerCharacter* PlayerCharacter =
        GetDRPlayerCharacter();

    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    PlayerCharacter->RequestSecondaryItemAction();
}

void ADRPlayerController::HandleSelectQuickSlot(
    const FInputActionValue& Value)
{
    const int32 InputSlotNumber =
        FMath::RoundToInt(Value.Get<float>());

    // 사용자에게 보이는 1번 슬롯은 배열 인덱스 0
    const int32 SlotIndex =
        InputSlotNumber - 1;

    if (SlotIndex < 0)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[QuickSlot Input] "
            "InputNumber=%d SlotIndex=%d"),
        InputSlotNumber,
        SlotIndex);
    
    QuickSlotComponent->RequestSelectSlot(SlotIndex);
}

#pragma region Terrain Dig
void ADRPlayerController::Client_ApplyTerrainDigHistory_Implementation(
    const TArray<FDRTerrainDigOperation>& DigHistory)
{
    // PostLogin 이후 받은 서버 지형 이력은 순서대로 TerrainSubsystem에 위임한다.
    for (const FDRTerrainDigOperation& Operation : DigHistory)
    {
        ApplyTerrainDigOnce(Operation);
    }
}

bool ADRPlayerController::ApplyTerrainDigOnce(
    const FDRTerrainDigOperation& Operation)
{
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return false;
    }

    UDRVoxelTerrainSubsystem* TerrainSubsystem =
        World->GetSubsystem<UDRVoxelTerrainSubsystem>();
    if (!IsValid(TerrainSubsystem))
    {
        return false;
    }

    // VoxelWorld가 아직 생성되지 않았다면 Subsystem이 delegate 기반 pending으로 보관한다.
    return TerrainSubsystem->ApplyOrQueueDig(Operation);
}
#pragma endregion
