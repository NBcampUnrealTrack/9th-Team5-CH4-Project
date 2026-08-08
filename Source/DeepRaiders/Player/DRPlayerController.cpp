#include "DRPlayerController.h"

#include "DRPlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"

void ADRPlayerController::BeginPlay()
{
    Super::BeginPlay();

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
    
    if (IsValid(MineAction.Get()))
    {
        EnhancedInput->BindAction(
            MineAction.Get(),
            ETriggerEvent::Started,
            this,
            &ThisClass::HandleMine);
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

void ADRPlayerController::HandleMine(
    const FInputActionValue& Value)
{
    ADRPlayerCharacter* PlayerCharacter =
        GetDRPlayerCharacter();

    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    PlayerCharacter->RequestMine();
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

    /*
     * QuickSlotComponent가 머지되면 여기서 호출
     *
     * QuickSlotComponent->RequestSelectSlot(SlotIndex);
     */
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
