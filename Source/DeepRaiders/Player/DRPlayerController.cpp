#include "DRPlayerController.h"

#include "DRPlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"

#include "DeepRaiders/Inventory/Component/DRInventoryComponent.h"
#include "Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DeepRaiders/Item/DRItemDefinition.h"
#include "DeepRaiders/Item/DRWorldItemActor.h"
#include "DeepRaiders/Item/Components/DRUsableDiggingComponent.h"
#include "DeepRaiders/Core/Subsystem/DRWorldItemSubsystem.h"
#include "DeepRaiders/OrePooling/DROrePoolActor.h"
#include "DeepRaiders/OrePooling/DROrePoolSubsystem.h"

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
    
    if (IsValid(InteractAction.Get()))
    {
        EnhancedInput->BindAction(
            InteractAction,
            ETriggerEvent::Started,
            this,
            &ThisClass::HandleInteract);
    }
    
    if (IsValid(DropHeldItemAction.Get()))
    {
        EnhancedInput->BindAction(
            DropHeldItemAction,
            ETriggerEvent::Started,
            this,
            &ThisClass::HandleDropHeldItem);
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
    
    if (QuickSlotComponent->TryBindFirstEmptySlot(StartingShovelDefinition))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "[StartingItem] Success Bind"));
    }

    // 아무 슬롯도 선택되지 않았다면 1번 선택
    if (QuickSlotComponent->GetSelectedSlotIndex() == INDEX_NONE)
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

void ADRPlayerController::HandleInteract(const FInputActionValue&)
{
    UE_LOG(LogTemp, Log, TEXT("Interact Called"));
    
    FHitResult Hit;
    
    // 상호작용 가능한 액터 탐색
    if (!IsLocalController()
        || !TraceInteractable(Hit))
    {
        return;
    }
    
    // Interface 구현 여부 확인
    AActor* Target = Hit.GetActor();
    if (!IsValid(Target)
        || !Target->Implements<UDRInteractableInterface>())
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
    
    if (!IsValid(CachedPawn)
        ||!IsValid(World))
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
    if (!IsValid(CachedPawn)
        || !IsValid(ExpectedTarget)
        || !ExpectedTarget->Implements<UDRInteractableInterface>()
        || !TraceInteractable(ServerHit)
        || ServerHit.GetActor() != ExpectedTarget)
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
    return HasAuthority() && IsValid(QuickSlotInventoryComponent) 
        && QuickSlotInventoryComponent->CanAddItem(Definition, Quantity);
}

bool ADRPlayerController::TryReceiveItem(UDRItemDefinition* Definition, int32 Quantity)
{
    // 퀵슬롯 여부와는 상관없이 아이템은 추가될 수 있다.
    if (!CanReceiveItem(Definition,Quantity) 
        || !QuickSlotInventoryComponent->TryAddItem(Definition, Quantity))
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
    RequestThrowHeldItem();
}

void ADRPlayerController::RequestThrowHeldItem()
{
    if (IsLocalController())
    {
        ServerRequestDropHeldItem();
    }
}

void ADRPlayerController::ServerRequestDropHeldItem_Implementation()
{
    // 버리기 초기 구현은 1개로 제한
    constexpr int32 DropQuantity = 1;
    
    APawn* CachedPawn = GetPawn();
    
    if (!HasAuthority()
        || !IsValid(CachedPawn)
        || !IsValid(QuickSlotComponent)
        || !IsValid(QuickSlotInventoryComponent))
    {
        return;
    }
    
    UDRItemDefinition* Definition = QuickSlotComponent->GetSelectedItemDefinition();
    
    if (!IsValid(Definition)
        || QuickSlotInventoryComponent->GetItemCount(Definition) < DropQuantity)
    {
        return;
    }
    
    const FVector Forward = CachedPawn->GetActorForwardVector();
    
    const FVector DropLocation = CachedPawn->GetActorLocation() + Forward * DropForwardDistance + FVector::UpVector * DropVerticalOffset;
    const FRotator DropRotation(0.0f, CachedPawn->GetActorRotation().Yaw, 0.0f);
    
    const FTransform BaseSpawnTransform(DropRotation, DropLocation);
    ADRWorldItemActor* DroppedItem = SpawnDroppedItem(Definition, BaseSpawnTransform, DropQuantity);
    
    if (!IsValid(DroppedItem))
    {
        return;
    }
    
    if (!QuickSlotInventoryComponent->TryRemoveItemByDefinition(Definition, DropQuantity))
    {
        // 아이템 차감 실패 시 롤백
        RollbackDroppedItem(DroppedItem);
        return;
    }
    
    DroppedItem->ApplyDropImpulse(Forward * DropImpulseStrength);
    ActivateUsableDiggingItem(DroppedItem);
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
        
        if (!IsValid(ActorClass)
            || !ActorClass->IsChildOf(ADROrePoolActor::StaticClass()))
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

void ADRPlayerController::ActivateUsableDiggingItem(
    ADRWorldItemActor* SpawnedItem) const
{
    if (!HasAuthority() || !IsValid(SpawnedItem))
    {
        return;
    }
    
    UDRUsableDiggingComponent* DiggingComponent = SpawnedItem->FindComponentByClass<UDRUsableDiggingComponent>();
    if (IsValid(DiggingComponent))
    {
        DiggingComponent->StartDigging();
    }
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