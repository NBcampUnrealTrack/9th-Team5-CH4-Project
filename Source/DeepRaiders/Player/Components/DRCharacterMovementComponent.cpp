#include "DRCharacterMovementComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "VoxelRender/VoxelProceduralMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Controller.h"


namespace
{
    FVector ResolveManualZiplineTraverseAxis(
        const FDRMovementActionState& State)
    {
        if (!State.IsActive()
            || State.ActionType != EDRMovementActionType::Zipline
            || State.ZiplineRideMode != EDRZiplineRideMode::ManualTraverse)
        {
            return FVector::ZeroVector;
        }

        return State.GetZiplineManualPositiveAxis();
    }

    int8 ResolveManualZiplineInput(
        const FDRMovementActionState& State,
        const FVector& Acceleration)
    {
        const FVector TraverseAxis =
            ResolveManualZiplineTraverseAxis(State);

        if (TraverseAxis.IsNearlyZero()
            || Acceleration.IsNearlyZero())
        {
            return 0;
        }

        const float InputDot =
            FVector::DotProduct(
                Acceleration.GetSafeNormal(),
                TraverseAxis);

        if (InputDot > KINDA_SMALL_NUMBER)
        {
            return 1;
        }

        if (InputDot < -KINDA_SMALL_NUMBER)
        {
            return -1;
        }

        return 0;
    }
}

class FSavedMove_DRCharacter : public FSavedMove_Character
{
public:
    typedef FSavedMove_Character Super;

    uint8 bSavedWantsJetpack : 1;
    uint8 bSavedZiplineActive : 1;
    int8 SavedManualZiplineInput = 0;
    float SavedZiplineRailSpeed = 0.f;
    float SavedJetpackSpoolElapsed = 0.f;

    virtual void Clear() override
    {
        Super::Clear();

        bSavedWantsJetpack = false;
        bSavedZiplineActive = false;
        SavedManualZiplineInput = 0;
        SavedZiplineRailSpeed = 0.f;
        SavedJetpackSpoolElapsed = 0.f;
    }

    virtual uint8 GetCompressedFlags() const override
    {
        uint8 Result = Super::GetCompressedFlags();

        if (bSavedWantsJetpack)
        {
            Result |= FLAG_Custom_0;
        }

        if (SavedManualZiplineInput > 0)
        {
            Result |= FLAG_Custom_1;
        }
        else if (SavedManualZiplineInput < 0)
        {
            Result |= FLAG_Custom_2;
        }

        return Result;
    }

    virtual bool CanCombineWith(
        const FSavedMovePtr& NewMove,
        ACharacter* InCharacter,
        float MaxDelta) const override
    {
        const FSavedMove_DRCharacter* NewDRMove = static_cast<const FSavedMove_DRCharacter*>(NewMove.Get());

        if (bSavedWantsJetpack != NewDRMove->bSavedWantsJetpack
            || SavedManualZiplineInput != NewDRMove->SavedManualZiplineInput)
        {
            return false;
        }

        /*
         * Zipline 가감속은 프레임 단위 rail-speed 적분을 사용한다.
         * Move를 합치면 같은 입력이어도 적분 결과가 달라질 수 있으므로
         * Zipline 탑승 중에는 SavedMove를 합치지 않는다.
         */
        if (bSavedZiplineActive
            || NewDRMove->bSavedZiplineActive)
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

        const UDRMovementActionComponent* MovementAction =
            Character->FindComponentByClass<UDRMovementActionComponent>();

        const FDRMovementActionState* ActionState =
            IsValid(MovementAction)
                ? &MovementAction->GetSimulationActionState()
                : nullptr;

        bSavedZiplineActive =
            ActionState != nullptr
            && ActionState->IsActive()
            && ActionState->ActionType
                == EDRMovementActionType::Zipline;

        SavedManualZiplineInput =
            ActionState != nullptr
                ? ResolveManualZiplineInput(
                    *ActionState,
                    NewAccel)
                : 0;
        /*
         * ZiplineRailSpeed는 활성 Zipline SavedMove에서만 의미가 있다.
         *
         * 비활성 Move에 이전 Zipline 속도를 저장하면 correction/replay 시
         * 종료된 세션의 RailSpeed가 다시 살아날 수 있다.
         */
        SavedZiplineRailSpeed =
            bSavedZiplineActive
                ? Movement->ZiplineRailSpeed
                : 0.f;

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

        /*
         * SavedMove의 Zipline transient state는
         * "그 SavedMove가 Zipline이었음"만으로 복원하면 안 된다.
         *
         * correction/replay 시 이미 서버 authoritative state에서
         * Zipline이 종료된 뒤 과거 Zipline Move를 replay할 수도 있다.
         *
         * 따라서 현재 simulation state와 MovementMode까지
         * 실제 Zipline 상태일 때만 복원한다.
         */
        const UDRMovementActionComponent* MovementAction =
            Character->FindComponentByClass<
                UDRMovementActionComponent>();

        const FDRMovementActionState* ActionState =
            IsValid(MovementAction)
                ? &MovementAction->GetSimulationActionState()
                : nullptr;

        const bool bCanRestoreZiplineState =
            bSavedZiplineActive
            && ActionState != nullptr
            && ActionState->IsActive()
            && ActionState->ActionType
                == EDRMovementActionType::Zipline
            && Movement->IsCustomMovementModeActive(
                EDRCustomMovementMode::MovementAction);

        if (bCanRestoreZiplineState)
        {
            Movement->ManualZiplineInput =
                SavedManualZiplineInput;

            Movement->ZiplineRailSpeed =
                SavedZiplineRailSpeed;
        }
        else
        {
            Movement->ManualZiplineInput = 0;
            Movement->ZiplineRailSpeed = 0.f;
        }

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

void UDRCharacterMovementComponent::OnMovementModeChanged(
	EMovementMode PreviousMovementMode,
	uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(
		PreviousMovementMode,
		PreviousCustomMode);

	const bool bWasMovementAction =
		PreviousMovementMode == MOVE_Custom
		&& PreviousCustomMode ==
			static_cast<uint8>(
				EDRCustomMovementMode::MovementAction);

	const bool bIsMovementAction =
		MovementMode == MOVE_Custom
		&& CustomMovementMode ==
			static_cast<uint8>(
				EDRCustomMovementMode::MovementAction);

	/*
	 * ExitCustomMovementMode()를 통하지 않고
	 * network correction / replicated movement가 직접
	 * MovementMode를 변경할 수도 있다.
	 *
	 * ZiplineRailSpeed와 ManualZiplineInput은
	 * MovementAction CustomMode 안에서만 유효한 transient state이므로
	 * 어떤 경로로 빠져나가든 여기서 반드시 정리한다.
	 */
	if (bWasMovementAction
		&& !bIsMovementAction)
	{
		ManualZiplineInput = 0;
		ZiplineRailSpeed = 0.f;
	}

	/*
	 * 진단 로그용 상태.
	 */
	const UDRMovementActionComponent* MovementAction =
		GetMovementActionComponent();

	const FDRMovementActionState* State =
		IsValid(MovementAction)
			? &MovementAction->GetSimulationActionState()
			: nullptr;

	const float WorldTime =
		GetWorld() != nullptr
			? GetWorld()->GetTimeSeconds()
			: -1.f;
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

void UDRCharacterMovementComponent::ResetManualZiplineInputState()
{
    ManualZiplineInput = 0;
    Acceleration = FVector::ZeroVector;

    if (IsValid(CharacterOwner))
    {
        CharacterOwner->ConsumeMovementInputVector();
    }
}

void UDRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    const bool bNewWantsJetpack =
        (Flags &
         FSavedMove_Character::FLAG_Custom_0) != 0;

    SetWantsJetpack(bNewWantsJetpack);


    const bool bManualZiplineForward =
        (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;

    const bool bManualZiplineBackward =
        (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;

    if (bManualZiplineForward == bManualZiplineBackward)
    {
        ManualZiplineInput = 0;
    }
    else
    {
        ManualZiplineInput =
            bManualZiplineForward ? 1 : -1;
    }
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
        ManualZiplineInput = 0;
        ZiplineRailSpeed = 0.f;
        return;
    }

    ManualZiplineInput = 0;
    ZiplineRailSpeed = 0.f;
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
    ManualZiplineInput = 0;
    ZiplineRailSpeed = 0.f;

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

bool UDRCharacterMovementComponent::TryHandleZiplineRiderCollision(const FHitResult& Hit)
{
    if (!IsValid(CharacterOwner) || !CharacterOwner->HasAuthority())
    {
        return false;
    }

    ACharacter* OtherCharacter = Cast<ACharacter>(Hit.GetActor());

    if (!IsValid(OtherCharacter) || OtherCharacter == CharacterOwner)
    {
        return false;
    }

    UDRMovementActionComponent* ThisAction = GetMovementActionComponent();
    UDRMovementActionComponent* OtherAction = OtherCharacter->FindComponentByClass<UDRMovementActionComponent>();
    UDRCharacterMovementComponent* OtherMovement = Cast<UDRCharacterMovementComponent>(OtherCharacter->GetCharacterMovement());
    if (!IsValid(ThisAction) || !IsValid(OtherAction) || !IsValid(OtherMovement))
    {
        return false;
    }

    /*
     * 단순 Character 충돌이 아니라,
     * 양쪽 모두 실제 Zipline 탑승 중일 때만
     * Rider Collision으로 처리한다.
     */
    if (!ThisAction->IsZiplineActive() || !OtherAction->IsZiplineActive())
    {
        return false;
    }

    /*
     * 서버가 두 Zipline Action을 동시에 종료한다.
     *
     * 한쪽만 종료하면 같은 충돌에서 상대는 계속 Rail을
     * 진행하므로 양쪽 모두 동일한 authoritative 결과를 갖게 한다.
     */
    ThisAction->EndMovementAction(EDRMovementActionEndReason::Collision);
    OtherAction->EndMovementAction(EDRMovementActionEndReason::Collision);

    /*
     * EndMovementAction은 Action State 종료이고,
     * 실제 CMC CustomMode도 별도로 빠져나와야 한다.
     */
    if (IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
    {
        ExitCustomMovementMode();
    }

    if (OtherMovement->IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
    {
        OtherMovement->ExitCustomMovementMode();
    }

    /*
     * ActionState 종료 + MovementMode 변경을
     * 가능한 빨리 각 클라이언트에 전달한다.
     */
    CharacterOwner->ForceNetUpdate();
    OtherCharacter->ForceNetUpdate();

    return true;
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

void UDRCharacterMovementComponent::UpdateZiplineFacing(
    const FDRMovementActionState& State,
    float DeltaTime)
{
    if (!UpdatedComponent
        || !State.IsActive()
        || State.ActionType != EDRMovementActionType::Zipline)
    {
        return;
    }

    FVector DesiredFacing;

    if (State.ZiplineRideMode == EDRZiplineRideMode::AutoTraverse)
    {
        // Auto는 선택된 실제 Cable 진행 Endpoint 방향을 바라본다.
        DesiredFacing =
            FVector(State.ReferenceLocation)
            - FVector(State.ZiplineStartLocation);

        FVector HorizontalFacing = DesiredFacing;
        HorizontalFacing.Z = 0.f;

        // 완전 수직 Auto에서는 진행축으로 yaw를 정할 수 없으므로
        // 탑승 순간 저장한 방향을 그대로 유지한다.
        if (HorizontalFacing.IsNearlyZero())
        {
            DesiredFacing =
                State.ZiplineFacingDirection;
        }
    }
    else if (State.ZiplineManualControlMode
        == EDRZiplineManualControlMode::ViewRelative)
    {
        /*
         * ViewRelative Manual은 실제 rail speed가 0을 지나 반대 부호가 된 뒤
         * 몸 방향을 진행 방향으로 뒤집는다.
         *
         * RailSpeed == 0인 감속/정지 구간에서는 현재 몸 방향을 유지하므로
         * 키를 바꾼 순간 바로 180도 튀지 않는다.
         */
        if (FMath::Abs(ZiplineRailSpeed)
            > KINDA_SMALL_NUMBER)
        {
            DesiredFacing =
                State.GetZiplineManualPositiveAxis()
                * FMath::Sign(ZiplineRailSpeed);
        }
        else
        {
            DesiredFacing =
                UpdatedComponent->GetForwardVector();
        }
    }
    else
    {
        /*
         * Vertical Manual:
         * - 이동 입력 의미는 기존 그대로 W=위 / S=아래.
         * - 몸의 yaw는 탑승 순간 world 방향으로 고정하지 않는다.
         * - 카메라(ControlRotation) yaw를 따라 Rope의 수직 축 주위를 자유롭게 360도 돈다.
         *
         * Gameplay Capsule은 Cable A-B 고정 rail에 있으므로 실제 이동선은 바뀌지 않고,
         * ABP의 Character-local RideOffset만 Character yaw와 함께 Rope 주위를 회전한다.
         *
         * 서버에도 owning Controller가 있으므로 authoritative yaw가 동일하게 계산되고,
         * 다른 클라이언트는 replicated Character rotation을 받아 같은 방향을 본다.
         */
        const AController* Controller =
            CharacterOwner
                ? CharacterOwner->GetController()
                : nullptr;

        if (IsValid(Controller))
        {
            const float ControlYaw =
                Controller->GetControlRotation().Yaw;

            DesiredFacing =
                FRotator(
                    0.f,
                    ControlYaw,
                    0.f)
                .Vector();
        }
        else
        {
            // Controller가 없는 proxy/fallback에서는 현재 회전을 그대로 유지한다.
            DesiredFacing =
                UpdatedComponent->GetForwardVector();
        }
    }

    DesiredFacing.Z = 0.f;
    DesiredFacing = DesiredFacing.GetSafeNormal();

    if (DesiredFacing.IsNearlyZero())
    {
        return;
    }

    const FRotator CurrentRotation =
        UpdatedComponent->GetComponentRotation();

    const float DesiredYaw =
        DesiredFacing.Rotation().Yaw;

    const float MaxYawStep =
        FMath::Max(
            FMath::Abs(RotationRate.Yaw),
            0.f)
        * FMath::Max(DeltaTime, 0.f);

    const float NewYaw =
        MaxYawStep > KINDA_SMALL_NUMBER
            ? FMath::FixedTurn(
                CurrentRotation.Yaw,
                DesiredYaw,
                MaxYawStep)
            : DesiredYaw;

    FRotator NewRotation = CurrentRotation;
    NewRotation.Pitch = 0.f;
    NewRotation.Yaw = NewYaw;
    NewRotation.Roll = 0.f;

    // Capsule은 yaw 회전에 대해 대칭이지만 CMC 경로를 통해 회전시켜
    // autonomous/server prediction 및 replicated movement 흐름을 유지한다.
    MoveUpdatedComponent(
        FVector::ZeroVector,
        NewRotation.Quaternion(),
        false);
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
        const FDRMovementActionState* State =
            IsValid(MovementAction)
                ? &MovementAction->GetSimulationActionState()
                : nullptr;

        const float WorldTime =
            GetWorld() != nullptr
                ? GetWorld()->GetTimeSeconds()
                : -1.f;

        RestoreDefaultMovementMode();

        if (HasValidData())
        {
            StartNewPhysics(
                DeltaTime,
                Iterations);
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
        
        /*
         * 중간 속도 적분에 사용할 시작 속도다.
         * 거리 제약을 시작/종료 속도 모두에 적용해야 이번 서브스텝부터 거리 감소가 반영된다.
         */
        FVector IntegrationStartVelocity = Velocity;
        
        FDRMovementActionSimulationInput Input;
        Input.Location = UpdatedComponent->GetComponentLocation();
        Input.Velocity = Velocity;
        Input.DeltaTime = TimeTick;
        // 기존 CharacterMovement의 공중 제어 계산을 재사용
        Input.InputAcceleration = GetFallingLateralAcceleration(TimeTick);
        Input.Gravity = -GetGravityDirection() * GetGravityZ();
        // 소유 클라이언트는 즉각적인 반응을 위해 현재 Acceleration을 사용한다.
        // Dedicated Server의 원격 Pawn은 SavedMove Custom Flag로 복원한
        // ManualZiplineInput을 사용해 W/S pressed/released를 정확히 재현한다.
        Input.RawAcceleration = Acceleration;
        Input.ZiplineRailSpeed = ZiplineRailSpeed;
        Input.ZiplineFacingDirection =
            UpdatedComponent->GetForwardVector();

        const FDRMovementActionState& ActionState =
            MovementAction->GetSimulationActionState();

        if (ActionState.ActionType == EDRMovementActionType::Zipline
            && ActionState.ZiplineRideMode == EDRZiplineRideMode::ManualTraverse
            && IsValid(CharacterOwner)
            && !CharacterOwner->IsLocallyControlled())
        {
            const FVector TraverseAxis =
                ResolveManualZiplineTraverseAxis(ActionState);

            Input.RawAcceleration =
                TraverseAxis * static_cast<float>(ManualZiplineInput);
        }
        
        FDRMovementActionSimulationOutput Output;
        MovementAction->EvaluateMovementContribution(Input, Output);

        if (Output.bUpdateZiplineRailSpeed)
        {
            ZiplineRailSpeed =
                Output.ZiplineRailSpeed;
        }

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

            /*
            * 그래플 가속도, 중력, MaxSpeed가 계산된 최종 속도에 기준점 제약을 적용한다.
            *
            * IntegrationStartVelocity도 함께 제약하지 않으면 중간 속도 적분에 이전의 바깥 방향
            * 속도가 남아 첫 서브스텝 동안 캐릭터가 계속 멀어질 수 있다.
            */
            MovementAction->ConstrainMovementVelocity(Input.Location, IntegrationStartVelocity);
            MovementAction->ConstrainMovementVelocity(Input.Location, Velocity);
            
            Adjusted = 0.5f * (IntegrationStartVelocity + Velocity) * TimeTick;
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
            
            /*
             * 서버에서 Zipline Rider끼리 Capsule Blocking Hit가 발생하면
             * 두 Rider의 Zipline을 즉시 종료한다.
             */
            if (TryHandleZiplineRiderCollision(Hit))
            {
                if (HasValidData())
                {
                    /*
                     * 충돌 시점 이후 남은 이번 substep 시간 +
                     * 아직 처리하지 않은 전체 RemainingTime을
                     * 새 MovementMode에서 계속 처리한다.
                     */
                    StartNewPhysics(
                        RemainingTime
                            + RemainingTimeAfterHit,
                        Iterations);
                }

                return;
            }
            
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

        UpdateZiplineFacing(
            ActionState,
            TimeTick);

        /*
         * Auto는 목표 Endpoint에 도달하면 완료 처리 후 자동 하차한다.
         * 진입 속도/Attach 보정 속도가 하차 직후 남지 않도록 먼저 0으로 정리한다.
         *
         * 서버는 EndMovementAction에서 authoritative state를 종료/복제하고,
         * owning client는 predicted state를 즉시 정리한다.
         */
        if (MovementAction->IsZiplineTargetReached(
                UpdatedComponent->GetComponentLocation()))
        {
            Velocity = FVector::ZeroVector;
            ZiplineRailSpeed = 0.f;

            MovementAction->ReportMovementSimulation(
                UpdatedComponent->GetComponentLocation(),
                Velocity);

            MovementAction->EndMovementAction(
                EDRMovementActionEndReason::Completed);

            RestoreDefaultMovementMode();

            if (IsValid(CharacterOwner)
                && CharacterOwner->HasAuthority())
            {
                // 최종 Endpoint transform + 새 MovementMode를 즉시 전송한다.
                CharacterOwner->ForceNetUpdate();
            }

            if (HasValidData())
            {
                StartNewPhysics(
                    RemainingTime,
                    Iterations);
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
