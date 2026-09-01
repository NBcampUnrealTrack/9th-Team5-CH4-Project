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

void UDRCharacterMovementComponent::OnMovementUpdated(
	float DeltaSeconds,
	const FVector& OldLocation,
	const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	OnCharacterMovementUpdated.Broadcast(DeltaSeconds, OldLocation, OldVelocity);
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

float UDRCharacterMovementComponent::GetMaxSpeed() const
{
    const float ConfiguredMaxSpeed = Super::GetMaxSpeed();
    
    if (MovementMode != MOVE_Falling)
    {
        return ConfiguredMaxSpeed;
    }
    
    // 이동 액션으로 얻은 현재 횡방향 속도는 Falling 진입 후에도 허용한다.
    // 현재 속도보다 높은 값을 새로 제공하지 않으므로 일반 점프의 최대 이동 속도는 그대로 유지된다.
    const float CurrentLateralSpeed = ProjectToGravityFloor(Velocity).Size();
    
    return FMath::Max(ConfiguredMaxSpeed, CurrentLateralSpeed);
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
    
    const FVector ExitVelocity = Velocity;
    
    FFindFloorResult FloorResult;
    FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
    
    const float FloorDistance = FloorResult.bLineTrace ? FloorResult.LineDist : FloorResult.FloorDist;
    const float VerticalSpeed = GetGravitySpaceZ(ExitVelocity);
    
    // FindFloor는 MaxStepHeight 범위까지 바닥을 찾을 수 있다.
    // 실제 Walking 허용 높이에 있고 바닥으로 이동 중일 때만 지상 이동으로 복귀한다.
    const bool bCanReturnToWalking = FloorResult.IsWalkableFloor() && FloorDistance <= MAX_FLOOR_DIST 
        && VerticalSpeed <= KINDA_SMALL_NUMBER;
    
    if (bCanReturnToWalking)
    {
        SetMovementMode(MOVE_Walking);
        return;
    }
    
    SetMovementMode(MOVE_Falling);
    
    // Custom Mode에서 계산된 속도를 Falling에 그대로 넘긴다.
    // 이후 중력과 공중 제어만 일반 PhysFalling 규칙으로 적용된다.
    Velocity = ExitVelocity;
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
        
        if (HasValidData())
        {
            // 이동 모드가 전환된 같은 프레임의 남은 시간도 새 물리 모드로 처리한다.
            StartNewPhysics(DeltaTime, Iterations);
        }
        
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
        // 가공하지 않은 원본 입력 가속도. Zipline ManualTraverse가 W/S 방향 판단에 사용한다.
        Input.RawAcceleration = Acceleration;
        
        FDRMovementActionSimulationOutput Output;
        MovementAction->EvaluateMovementContribution(Input, Output);

        FVector Adjusted;

        if (Output.bOverrideVelocity)
        {
            // Zipline처럼 기존 속도/중력/가속을 완전히 무시하는 액션 전용 경로다.
            Velocity = Output.OverrideVelocity;
            Adjusted = Velocity * TimeTick;
        }
        else
        {
            Velocity += Output.AdditionalAcceleration * TimeTick;

            if (Output.bApplyGravity)
            {
                Velocity = NewFallVelocity(Velocity, Input.Gravity, TimeTick);
            }

            if (Output.MaxSpeed > KINDA_SMALL_NUMBER)
            {
                Velocity = Velocity.GetClampedToMaxSize(Output.MaxSpeed);
            }

            Adjusted = 0.5f * (OldVelocity + Velocity) * TimeTick;
        }

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

        // Zipline 목표 Endpoint에 도달하면 즉시 액션을 종료하고 기존 이동 모드로 복귀한다.
        if (MovementAction->IsZiplineTargetReached(UpdatedComponent->GetComponentLocation()))
        {
            MovementAction->EndMovementAction(EDRMovementActionEndReason::Completed);

            RestoreDefaultMovementMode();

            if (HasValidData())
            {
                StartNewPhysics(RemainingTime, Iterations);
            }

            return;
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
