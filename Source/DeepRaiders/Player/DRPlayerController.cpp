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
    const FInputActionValue& Value)
{
    ADRPlayerCharacter* PlayerCharacter =
        GetDRPlayerCharacter();

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->Jump();
    }
}

void ADRPlayerController::HandleJumpCompleted(
    const FInputActionValue& Value)
{
    ADRPlayerCharacter* PlayerCharacter =
        GetDRPlayerCharacter();

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->StopJumping();
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
