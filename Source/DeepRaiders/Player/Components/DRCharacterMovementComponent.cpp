#include "DRCharacterMovementComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "VoxelRender/VoxelProceduralMeshComponent.h"
#include "AbilitySystemComponent.h"

class FSavedMove_DRCharacter : public FSavedMove_Character
{
public:
    typedef FSavedMove_Character Super;

    uint8 bSavedWantsJetpack : 1;
    float SavedJetpackSpoolElapsed = 0.f;

    virtual void Clear() override
    {
        Super::Clear();

        bSavedWantsJetpack = false;
        SavedJetpackSpoolElapsed = 0.f;
    }

    virtual uint8 GetCompressedFlags() const override
    {
        uint8 Result = Super::GetCompressedFlags();

        if (bSavedWantsJetpack)
        {
            Result |= FLAG_Custom_0;
        }

        return Result;
    }

    virtual bool CanCombineWith(
        const FSavedMovePtr& NewMove,
        ACharacter* InCharacter,
        float MaxDelta) const override
    {
        const FSavedMove_DRCharacter* NewDRMove = static_cast<const FSavedMove_DRCharacter*>(NewMove.Get());

        if (bSavedWantsJetpack != NewDRMove->bSavedWantsJetpack)
        {
            return false;
        }

        /*
         * 출력 증가 중인 이동을 합치면 프레임 단위 곡선 계산이
         * 달라질 수 있으므로 제트팩 사용 중에는 합치지 않는다.
         */
        if (bSavedWantsJetpack)
        {
            return false;
        }

        return Super::CanCombineWith(
            NewMove,
            InCharacter,
            MaxDelta);
    }

    virtual void SetMoveFor(
        ACharacter* Character,
        float InDeltaTime,
        const FVector& NewAccel,
        FNetworkPredictionData_Client_Character& ClientData) override
    {
        Super::SetMoveFor(
            Character,
            InDeltaTime,
            NewAccel,
            ClientData);

        const UDRCharacterMovementComponent* Movement = Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());

        if (!IsValid(Movement))
        {
            return;
        }

        bSavedWantsJetpack = Movement->bWantsJetpack;

        SavedJetpackSpoolElapsed = Movement->JetpackSpoolElapsed;
    }

    virtual void PrepMoveFor(
        ACharacter* Character) override
    {
        Super::PrepMoveFor(Character);

        UDRCharacterMovementComponent* Movement =
            Cast<UDRCharacterMovementComponent>(
                Character->GetCharacterMovement());

        if (!IsValid(Movement))
        {
            return;
        }

        Movement->bWantsJetpack =
            bSavedWantsJetpack;

        Movement->JetpackSpoolElapsed =
            SavedJetpackSpoolElapsed;
    }
};

class FNetworkPredictionData_Client_DRCharacter : public FNetworkPredictionData_Client_Character
{
public:
    explicit FNetworkPredictionData_Client_DRCharacter(const UCharacterMovementComponent& ClientMovement)
        : FNetworkPredictionData_Client_Character(ClientMovement)
    {
    }

    virtual FSavedMovePtr AllocateNewMove() override
    {
        return FSavedMovePtr(new FSavedMove_DRCharacter());
    }
};

UDRCharacterMovementComponent::UDRCharacterMovementComponent()
    : bWantsJetpack(false)
{
    GravityScale = 1.0f;
}

void UDRCharacterMovementComponent::BindAbilitySystem(
    UAbilitySystemComponent* AbilitySystemComponent)
{
    UnbindAbilitySystem();

    if (!IsValid(AbilitySystemComponent))
    {
        return;
    }

    BoundAbilitySystemComponent = AbilitySystemComponent;

    if (BaseWalkSpeed <= 0.f)
    {
        BaseWalkSpeed = MaxWalkSpeed;
    }

    MoveSpeedChangedDelegateHandle =
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
            UDRPlayerAttributeSet::GetMoveSpeedMultiplierAttribute())
        .AddUObject(
            this,
            &ThisClass::HandleMoveSpeedMultiplierChanged);

    ApplyMoveSpeedMultiplier(
        AbilitySystemComponent->GetNumericAttribute(
            UDRPlayerAttributeSet::GetMoveSpeedMultiplierAttribute()));
}

void UDRCharacterMovementComponent::ActivateSuperJumpAirControl(float NewAirControl)
{
    if (!bSuperJumpAirControlActive)
    {
        AirControlBeforeSuperJump = AirControl;
        bSuperJumpAirControlActive = true;
    }

    AirControl = FMath::Max(AirControl, NewAirControl);
}

void UDRCharacterMovementComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    UnbindAbilitySystem();
    Super::EndPlay(EndPlayReason);
}

void UDRCharacterMovementComponent::UnbindAbilitySystem()
{
    if (BoundAbilitySystemComponent.IsValid()
        && MoveSpeedChangedDelegateHandle.IsValid())
    {
        BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
            UDRPlayerAttributeSet::GetMoveSpeedMultiplierAttribute())
        .Remove(MoveSpeedChangedDelegateHandle);
    }

    MoveSpeedChangedDelegateHandle.Reset();
    BoundAbilitySystemComponent.Reset();
}

void UDRCharacterMovementComponent::HandleMoveSpeedMultiplierChanged(
    const FOnAttributeChangeData& Data)
{
    ApplyMoveSpeedMultiplier(Data.NewValue);
}

void UDRCharacterMovementComponent::ApplyMoveSpeedMultiplier(float Multiplier)
{
    MaxWalkSpeed = BaseWalkSpeed * FMath::Max(0.f, Multiplier);
}

void UDRCharacterMovementComponent::SetWantsJetpack(
    bool bNewWantsJetpack)
{
    if (bWantsJetpack == bNewWantsJetpack)
    {
        return;
    }

    bWantsJetpack = bNewWantsJetpack;
    JetpackSpoolElapsed = 0.f;
}

void UDRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    const bool bNewWantsJetpack =
        (Flags &
         FSavedMove_Character::FLAG_Custom_0) != 0;

    SetWantsJetpack(bNewWantsJetpack);
}

void UDRCharacterMovementComponent::SetBase(
    UPrimitiveComponent* NewBase,
    const FName BoneName,
    bool bNotifyActor)
{
    // CharacterMovement는 동적 MovementBase를 ServerMove에 실어 보낸다.
    // Voxel 런타임 메시 컴포넌트는 NetGUID를 지원하지 않으므로
    // MovementBase로 잡히면 FNetGUIDCache::SupportsObject 경고가 발생한다.
    if (NewBase && NewBase->IsA<UVoxelProceduralMeshComponent>())
    {
        Super::SetBase(nullptr, NAME_None, bNotifyActor);
        return;
    }

    Super::SetBase(
        NewBase,
        BoneName,
        bNotifyActor);
}

void UDRCharacterMovementComponent::ProcessLanded(
    const FHitResult& Hit,
    float RemainingTime,
    int32 Iterations)
{
    if (bSuperJumpAirControlActive)
    {
        AirControl = AirControlBeforeSuperJump;
        bSuperJumpAirControlActive = false;
    }

    Super::ProcessLanded(Hit, RemainingTime, Iterations);
}

FNetworkPredictionData_Client* UDRCharacterMovementComponent::GetPredictionData_Client() const
{
    if (ClientPredictionData == nullptr)
    {
        UDRCharacterMovementComponent* MutableThis =
            const_cast<
                UDRCharacterMovementComponent*>(this);

        MutableThis->ClientPredictionData =
            new FNetworkPredictionData_Client_DRCharacter(
                *this);
    }

    return ClientPredictionData;
}

void UDRCharacterMovementComponent::SetCustomMovementMode(EDRCustomMovementMode NewMode)
{
    if (NewMode == EDRCustomMovementMode::None)
    {
        ExitCustomMovementMode();
        return;
    }
    
    SetMovementMode(MOVE_Custom, static_cast<uint8>(NewMode)); 
}

void UDRCharacterMovementComponent::ExitCustomMovementMode()
{
    if (MovementMode != MOVE_Custom)
    {
        return;
    }
    
    RestoreDefaultMovementMode();
}

bool UDRCharacterMovementComponent::IsCustomMovementModeActive(EDRCustomMovementMode Mode) const
{
    return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(Mode);
}

UDRMovementActionComponent* UDRCharacterMovementComponent::GetMovementActionComponent() const
{
    if (!IsValid(CharacterOwner))
    {
        return nullptr;
    }
    
    return CharacterOwner->FindComponentByClass<UDRMovementActionComponent>();
}

void UDRCharacterMovementComponent::RestoreDefaultMovementMode()
{
    if (!UpdatedComponent)
    {
        SetMovementMode(MOVE_Falling);
        return;
    }
    
    FFindFloorResult FloorResult;
    FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
    
    if (FloorResult.IsWalkableFloor())
    {
        SetMovementMode(MOVE_Walking);
        return;
    }
    
    SetMovementMode(MOVE_Falling);
}

void UDRCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
    switch (static_cast<EDRCustomMovementMode>(CustomMovementMode))
    {
    case EDRCustomMovementMode::MovementAction:
        PhysMovementAction(deltaTime, Iterations);
        return;
    default:
        RestoreDefaultMovementMode();
        return;
    }
}

void UDRCharacterMovementComponent::PhysMovementAction(float DeltaTime, int32 Iterations)
{
    if (DeltaTime < MIN_TICK_TIME)
    {
        return;
    }
    
    UDRMovementActionComponent* MovementAction = GetMovementActionComponent();
    
    if (!IsValid(MovementAction)
        || !UpdatedComponent
        || !MovementAction->IsMovementActionActive())
    {
        RestoreDefaultMovementMode();
        return;
    }
    
    float RemainingTime = DeltaTime;
    
    while (RemainingTime >= MIN_TICK_TIME
        && Iterations < MaxSimulationIterations)
    {
        ++Iterations;
        
        const float TimeTick = GetSimulationTimeStep(RemainingTime, Iterations);
        
        RemainingTime -= TimeTick;
        bJustTeleported = false;
        
        const FVector OldVelocity = Velocity;
        
        FDRMovementActionSimulationInput Input;
        Input.Location = UpdatedComponent->GetComponentLocation();
        Input.Velocity = Velocity;
        Input.DeltaTime = TimeTick;
        // 기존 CharacterMovement의 공중 제어 계산을 재사용
        Input.InputAcceleration = GetFallingLateralAcceleration(TimeTick);
        Input.Gravity = -GetGravityDirection() * GetGravityZ();
        
        FDRMovementActionSimulationOutput Output;
        MovementAction->EvaluateMovementContribution(Input, Output);
    
        Velocity += Output.AdditionalAcceleration * TimeTick;
    
        if (Output.bApplyGravity)
        {
            Velocity = NewFallVelocity(Velocity, Input.Gravity, TimeTick);
        }
    
        if (Output.MaxSpeed > KINDA_SMALL_NUMBER)
        {
            Velocity = Velocity.GetClampedToMaxSize(Output.MaxSpeed);
        }
        
        const FVector Adjusted = 0.5f * (OldVelocity + Velocity) * TimeTick;
        FHitResult Hit(1.f);
        SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);
    
        if (!HasValidData())
        {
            return;
        }
        
        if (Hit.IsValidBlockingHit())
        {
            const float RemainingTimeAfterHit = TimeTick * (1.f - Hit.Time);
            const FVector SlideStartLocation = UpdatedComponent->GetComponentLocation();
            
            HandleImpact(Hit, TimeTick, Adjusted);
            
            if (!HasValidData()
                || !IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
            {
                return;
            }
            
            // 충돌면을 따라 남은 이동량 처리
            SlideAlongSurface(Adjusted, 1.f - Hit.Time, Hit.Normal, Hit, true);
            
            if (RemainingTimeAfterHit > KINDA_SMALL_NUMBER 
                && !bJustTeleported)
            {
                const FVector SlideDelta = UpdatedComponent->GetComponentLocation() - SlideStartLocation;
                
                // 다음 프레임에도 벽 안쪽으로 속도가 유지되지 않도록 보정
                Velocity = SlideDelta / RemainingTimeAfterHit;
            }
        }
    }
    
    if (IsValid(MovementAction)
        && MovementAction->IsMovementActionActive()
        && IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
    {
        MovementAction->ReportMovementSimulation(UpdatedComponent->GetComponentLocation(),Velocity);
    }    
}

bool UDRCharacterMovementComponent::CanApplyJetpackThrust() const
{
    if (!bWantsJetpack ||
        !IsFalling() ||
        !IsValid(CharacterOwner))
    {
        return false;
    }

    const ADRPlayerCharacter* PlayerCharacter =
        Cast<ADRPlayerCharacter>(CharacterOwner);

    if (!IsValid(PlayerCharacter) ||
        PlayerCharacter->IsDead())
    {
        return false;
    }

    const ADRPlayerState* PlayerState =
        PlayerCharacter
            ->GetPlayerState<ADRPlayerState>();

    if (!IsValid(PlayerState))
    {
        return false;
    }

    return PlayerState->HasJetpack() &&
        PlayerState->GetJetpackFuel() >
            KINDA_SMALL_NUMBER;
}

void UDRCharacterMovementComponent::PhysFalling(
    float DeltaTime,
    int32 Iterations)
{
    if (CanApplyJetpackThrust())
    {
        const float SafeSpoolUpTime =
            FMath::Max(
                JetpackSpoolUpTime,
                KINDA_SMALL_NUMBER);

        JetpackSpoolElapsed =
            FMath::Min(
                JetpackSpoolElapsed + DeltaTime,
                SafeSpoolUpTime);

        const float SpoolAlpha =
            FMath::Clamp(
                JetpackSpoolElapsed /
                    SafeSpoolUpTime,
                0.f,
                1.f);

        const float ThrustAlpha =
            FMath::Pow(
                SpoolAlpha,
                FMath::Max(
                    JetpackThrustExponent,
                    0.01f));

        const float CurrentAcceleration =
            FMath::Lerp(
                InitialJetpackAcceleration,
                MaxJetpackAcceleration,
                ThrustAlpha);

        Velocity.Z =
            FMath::Min(
                Velocity.Z +
                    CurrentAcceleration * DeltaTime,
                MaxJetpackRiseSpeed);
    }
    else
    {
        JetpackSpoolElapsed = 0.f;
    }

    /*
     * 중력, 충돌, 낙하 이동은 기존 CharacterMovement 로직에 맡긴다.
     */
    Super::PhysFalling(
        DeltaTime,
        Iterations);
}
