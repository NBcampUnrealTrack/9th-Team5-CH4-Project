#include "DRCharacterMovementComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "Components/CapsuleComponent.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelRender/VoxelProceduralMeshComponent.h"
#include "VoxelWorld.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
#include "Components/StaticMeshComponent.h"

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
    uint8 bSavedAirborneMomentumPreservationActive : 1;
    float SavedPreservedLateralSpeed = 0.f;
    
    virtual void Clear() override
    {
        Super::Clear();

        bSavedWantsJetpack = false;
        bSavedZiplineActive = false;
        bSavedAirborneMomentumPreservationActive = false;
        SavedManualZiplineInput = 0;
        SavedZiplineRailSpeed = 0.f;
        SavedJetpackSpoolElapsed = 0.f;
        SavedPreservedLateralSpeed = 0.f;
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
            || SavedManualZiplineInput != NewDRMove->SavedManualZiplineInput
            || bSavedAirborneMomentumPreservationActive != NewDRMove->bSavedAirborneMomentumPreservationActive)
        {
            return false;
        }

        // 보존 속도가 다른 Move를 합치면 correction replay에서 잘못된 속도 상한을 사용할 수 있다.
        if (bSavedAirborneMomentumPreservationActive
            && !FMath::IsNearlyEqual(SavedPreservedLateralSpeed,NewDRMove->SavedPreservedLateralSpeed))
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
        bSavedAirborneMomentumPreservationActive = Movement->bAirborneMomentumPreservationActive;

        SavedPreservedLateralSpeed = bSavedAirborneMomentumPreservationActive
            ? Movement->PreservedLateralSpeed : 0.f;
        
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

        /*
        * 서버에는 GA가 동일한 상태를 독립적으로 생성한다.
        * SavedMove 값은 소유 클라이언트가 correction replay를 수행할 때 과거 프레임 상태를 복원한다.
        */
        Movement->bAirborneMomentumPreservationActive = bSavedAirborneMomentumPreservationActive;

        Movement->PreservedLateralSpeed = bSavedAirborneMomentumPreservationActive
            ? FMath::Max(SavedPreservedLateralSpeed, 0.f) : 0.f;
        
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

void UDRCharacterMovementComponent::EnterVoxelContainedMode()
{
    StopMovementImmediately();
    ClearAccumulatedForces();
    SetMovementMode(
        MOVE_Custom,
        static_cast<uint8>(EDRCustomMovementMode::VoxelContained));

    if (IsValid(CharacterOwner) && CharacterOwner->HasAuthority())
    {
        CharacterOwner->ForceNetUpdate();
    }
}

void UDRCharacterMovementComponent::ExitVoxelContainedMode()
{
    if (!IsCustomMovementModeActive(EDRCustomMovementMode::VoxelContained))
    {
        return;
    }
    RestoreDefaultMovementMode();
}

void UDRCharacterMovementComponent::OnMovementUpdated(
	float DeltaSeconds,
	const FVector& OldLocation,
	const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
    
    // RepNotify 이후 추가 보정으로 MovementMode가 다시 변경된 경우에도 상태 불일치를 복구한다.
    ReconcileMovementActionMode();
    
	OnCharacterMovementUpdated.Broadcast(DeltaSeconds, OldLocation, OldVelocity);
}

bool UDRCharacterMovementComponent::CheckFall(
    const FFindFloorResult& OldFloor,
    const FHitResult& Hit,
    const FVector& Delta,
    const FVector& OldLocation,
    float RemainingTime,
    float TimeTick,
    int32 Iterations,
    bool bMustJump)
{
    // Voxel 데이터는 발밑이 고체라고 하지만 collision floor만 일시적으로
    // 사라진 경우에는 Falling 전환을 시작하지 않는다.
    if (ShouldKeepVoxelFloor(OldFloor, OldLocation))
    {
        return false;
    }

    return Super::CheckFall(
        OldFloor,
        Hit,
        Delta,
        OldLocation,
        RemainingTime,
        TimeTick,
        Iterations,
        bMustJump);
}

bool UDRCharacterMovementComponent::ShouldKeepVoxelFloor(
    const FFindFloorResult& OldFloor,
    const FVector& OldLocation) const
{
    if (!OldFloor.IsWalkableFloor() || !IsValid(CharacterOwner))
    {
        return false;
    }

    AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(OldFloor.HitResult.GetActor());
    if (!IsValid(VoxelWorld) && IsValid(OldFloor.HitResult.GetComponent()))
    {
        VoxelWorld = Cast<AVoxelWorld>(OldFloor.HitResult.GetComponent()->GetOwner());
    }
    if (!IsValid(VoxelWorld))
    {
        VoxelWorld = LastVoxelFloorWorld.Get();
    }
    if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
    {
        return false;
    }

    const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
    if (!IsValid(Capsule))
    {
        return false;
    }

    const FVector fGravityDirection = GetGravityDirection();
    const FVector OldCapsuleBottom =
        OldLocation + fGravityDirection * Capsule->GetScaledCapsuleHalfHeight();
    const FVector LocalOldBottom = VoxelWorld->GlobalToLocalFloat(OldCapsuleBottom).ToFloat();
    const FVoxelIntBox WorldBounds = VoxelWorld->GetWorldBounds();
    const float Tolerance = FMath::Max(0.f, VoxelLowerBoundaryTolerance);

    const bool bInsideHorizontalBounds =
        LocalOldBottom.X >= WorldBounds.Min.X && LocalOldBottom.X < WorldBounds.Max.X &&
        LocalOldBottom.Y >= WorldBounds.Min.Y && LocalOldBottom.Y < WorldBounds.Max.Y;

    if (bInsideHorizontalBounds && LocalOldBottom.Z <= WorldBounds.Min.Z + Tolerance)
    {
        return true;
    }

    if (!UpdatedComponent)
    {
        return false;
    }

    // OldLocation이 아닌 현재 캡슐 중심을 사용한다. 그렇지 않으면 정상적으로
    // 복셀 절벽을 걸어 나갈 때도 이전 바닥을 지지로 오판할 수 있다.
    const FVector CurrentCapsuleBottom =
        UpdatedComponent->GetComponentLocation() +
        fGravityDirection * Capsule->GetScaledCapsuleHalfHeight();
    const float ProbeRadius = Capsule->GetScaledCapsuleRadius() * 0.5f;
    const FVector Forward = CharacterOwner->GetActorForwardVector();
    const FVector Right = CharacterOwner->GetActorRightVector();
    const FVector ColumnOffsets[] =
    {
        FVector::ZeroVector,
        Forward * ProbeRadius,
        -Forward * ProbeRadius,
        Right * ProbeRadius,
        -Right * ProbeRadius
    };
    constexpr float ProbeDepthsInVoxels[] = { 0.25f, 0.75f, 1.25f };
    const FVector LocalGravityDirection =
        (VoxelWorld->GlobalToLocalFloat(CurrentCapsuleBottom + fGravityDirection) -
         VoxelWorld->GlobalToLocalFloat(CurrentCapsuleBottom))
        .ToFloat()
        .GetSafeNormal();
    if (LocalGravityDirection.IsNearlyZero())
    {
        return false;
    }

    FIntVector SamplePositions[UE_ARRAY_COUNT(ColumnOffsets)][UE_ARRAY_COUNT(ProbeDepthsInVoxels)];
    FVoxelIntBoxWithValidity LockBounds;
    for (int32 ColumnIndex = 0; ColumnIndex < UE_ARRAY_COUNT(ColumnOffsets); ++ColumnIndex)
    {
        for (int32 DepthIndex = 0; DepthIndex < UE_ARRAY_COUNT(ProbeDepthsInVoxels); ++DepthIndex)
        {
            const FVector LocalColumn =
                VoxelWorld->GlobalToLocalFloat(CurrentCapsuleBottom + ColumnOffsets[ColumnIndex]).ToFloat();
            const FVector LocalSample =
                LocalColumn + LocalGravityDirection * ProbeDepthsInVoxels[DepthIndex];
            const FIntVector VoxelPosition(
                FMath::RoundToInt(LocalSample.X),
                FMath::RoundToInt(LocalSample.Y),
                FMath::RoundToInt(LocalSample.Z));
            SamplePositions[ColumnIndex][DepthIndex] = VoxelPosition;
            if (WorldBounds.Contains(VoxelPosition))
            {
                LockBounds += VoxelPosition;
            }
        }
    }

    if (!LockBounds.IsValid())
    {
        return false;
    }

    FVoxelData& Data = VoxelWorld->GetData();
    FVoxelReadScopeLock Lock(Data, LockBounds.GetBox(), FUNCTION_FNAME);

    int32 SolidColumnCount = 0;
    for (int32 ColumnIndex = 0; ColumnIndex < UE_ARRAY_COUNT(ColumnOffsets); ++ColumnIndex)
    {
        bool bColumnHasSolidSupport = false;
        for (int32 DepthIndex = 0; DepthIndex < UE_ARRAY_COUNT(ProbeDepthsInVoxels); ++DepthIndex)
        {
            const FIntVector& VoxelPosition = SamplePositions[ColumnIndex][DepthIndex];
            if (WorldBounds.Contains(VoxelPosition) && !Data.GetValue(VoxelPosition, 0).IsEmpty())
            {
                bColumnHasSolidSupport = true;
                break;
            }
        }

        SolidColumnCount += bColumnHasSolidSupport ? 1 : 0;
    }

    // 발가락 정도의 일부 접촉으로는 이동을 막지 않고, 캡슐 하단의
    // 다수 영역에 실제 고체 voxel이 있을 때만 floor 소실을 무시한다.
    return SolidColumnCount >= 3;
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
    * 그래플 후속 관성은 Falling에서만 유효하다.
    * Walking, Swimming 또는 다른 이동 모드로 바뀌면 이전 속도 상한을 재사용하지 않는다.
    */
    if (bAirborneMomentumPreservationActive && MovementMode != MOVE_Falling)
    {
        ClearAirborneMomentumPreservation();
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

    VoxelContainedTagChangedDelegateHandle =
        AbilitySystemComponent->RegisterGameplayTagEvent(
            DRGameplayTags::State_VoxelContained,
            EGameplayTagEventType::NewOrRemoved)
        .AddUObject(
            this,
            &ThisClass::HandleVoxelContainedTagChanged);

    ApplyMoveSpeedMultiplier(
        AbilitySystemComponent->GetNumericAttribute(
            UDRPlayerAttributeSet::GetMoveSpeedMultiplierAttribute()));

    HandleVoxelContainedTagChanged(
        DRGameplayTags::State_VoxelContained,
        AbilitySystemComponent->GetTagCount(
            DRGameplayTags::State_VoxelContained));
}

void UDRCharacterMovementComponent::ActivateSuperJumpAirControl(
    float NewAirControl,
    float NewMaxAirSpeedMultiplier)
{
	++SuperJumpSequence;
	if (SuperJumpSequence == 0)
	{
		++SuperJumpSequence;
	}

    if (!bSuperJumpAirControlActive)
    {
        AirControlBeforeSuperJump = AirControl;
        bSuperJumpAirControlActive = true;
    }

    AirControl = FMath::Clamp(NewAirControl, 0.f, 1.f);
    SuperJumpMaxAirSpeedMultiplier = FMath::Clamp(
        NewMaxAirSpeedMultiplier,
        0.f,
        1.f);
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

    if (BoundAbilitySystemComponent.IsValid()
        && VoxelContainedTagChangedDelegateHandle.IsValid())
    {
        BoundAbilitySystemComponent->RegisterGameplayTagEvent(
            DRGameplayTags::State_VoxelContained,
            EGameplayTagEventType::NewOrRemoved)
        .Remove(VoxelContainedTagChangedDelegateHandle);
    }

    MoveSpeedChangedDelegateHandle.Reset();
	VoxelContainedTagChangedDelegateHandle.Reset();
    BoundAbilitySystemComponent.Reset();
}

void UDRCharacterMovementComponent::HandleMoveSpeedMultiplierChanged(
    const FOnAttributeChangeData& Data)
{
    ApplyMoveSpeedMultiplier(Data.NewValue);
}

void UDRCharacterMovementComponent::HandleVoxelContainedTagChanged(
	const FGameplayTag CallbackTag,
	int32 NewCount)
{
	if (NewCount > 0)
	{
		EnterVoxelContainedMode();
		return;
	}

	ExitVoxelContainedMode();
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
        LastVoxelFloorWorld = Cast<AVoxelWorld>(NewBase->GetOwner());
        Super::SetBase(nullptr, NAME_None, bNotifyActor);
        return;
    }

    LastVoxelFloorWorld.Reset();
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
        SuperJumpMaxAirSpeedMultiplier = 1.f;
        bSuperJumpAirControlActive = false;
    }
    
    // 지면 충돌이 그래플 후속 관성 상태의 명확한 종료 지점이다.
    ClearAirborneMomentumPreservation();

    Super::ProcessLanded(Hit, RemainingTime, Iterations);
	OnCharacterLanded.Broadcast(Hit);
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
    float ConfiguredMaxSpeed = Super::GetMaxSpeed();
    if (MovementMode == MOVE_Falling && bSuperJumpAirControlActive)
    {
        ConfiguredMaxSpeed *= SuperJumpMaxAirSpeedMultiplier;
    }
    
    if (MovementMode != MOVE_Falling || !bAirborneMomentumPreservationActive)
    {
        return ConfiguredMaxSpeed;
    }
    
    /*
     * 현재 속도를 매 프레임 읽지 않고 그래플 종료 순간 저장한 값만 사용한다.
     * 따라서 일반 점프, Dash, Jetpack의 모든 Falling에 전역으로 관성 보존이 적용되지 않는다.
     */
    return FMath::Max(ConfiguredMaxSpeed, PreservedLateralSpeed);
}

void UDRCharacterMovementComponent::BeginAirborneMomentumPreservation()
{
    /*
     * 수직 속도는 Falling 중력 계산에 맡긴다.
     * 여기서는 MaxWalkSpeed에 의해 갑자기 잘리던 중력 평면상의 속도만 저장한다.
     */
    PreservedLateralSpeed = ProjectToGravityFloor(Velocity).Size();
    bAirborneMomentumPreservationActive = PreservedLateralSpeed > KINDA_SMALL_NUMBER;
}

void UDRCharacterMovementComponent::ClearAirborneMomentumPreservation()
{
    bAirborneMomentumPreservationActive = false;
    PreservedLateralSpeed = 0.f;
}

bool UDRCharacterMovementComponent::ApplyKnockback(
	const FVector& Direction, float Distance, float Duration, float ElapsedTime)
{
	const bool bIsAutonomousClient = IsValid(CharacterOwner)
		&& CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy
		&& CharacterOwner->IsLocallyControlled();
	const bool bCanSimulateKnockback = IsValid(CharacterOwner)
		&& (CharacterOwner->HasAuthority() || bIsAutonomousClient);
	const float SafeDuration = FMath::Max(Duration, 0.01f);
	const float SafeElapsedTime = FMath::Max(ElapsedTime, 0.f);
	if (!IsValid(CharacterOwner)
		|| !bCanSimulateKnockback
		|| Distance <= KINDA_SMALL_NUMBER
		|| Direction.ContainsNaN()
		|| SafeElapsedTime >= SafeDuration
		|| MovementMode == MOVE_None
		|| IsCustomMovementModeActive(EDRCustomMovementMode::VoxelContained))
	{
		return false;
	}

	const FVector KnockbackDirection = Direction.GetSafeNormal();
	if (KnockbackDirection.IsNearlyZero())
	{
		return false;
	}

	if (UDRMovementActionComponent* MovementAction = GetMovementActionComponent();
		IsValid(MovementAction) && MovementAction->IsMovementActionActive())
	{
		MovementAction->EndMovementAction(EDRMovementActionEndReason::Cancelled);
	}

	if (IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
	{
		ExitCustomMovementMode();
	}

	if (BoundAbilitySystemComponent.IsValid())
	{
		FGameplayTagContainer InterruptedAbilityTags;
		InterruptedAbilityTags.AddTag(DRGameplayTags::Ability_Skill_CombatRoll);
		BoundAbilitySystemComponent->CancelAbilities(&InterruptedAbilityTags);
	}

	static const FName KnockbackRootMotionSourceName(TEXT("DRKnockback"));
	RemoveRootMotionSource(KnockbackRootMotionSourceName);

	StopMovementImmediately();
	ClearAccumulatedForces();
	ClearAirborneMomentumPreservation();

	const FVector StartLocation = CharacterOwner->GetActorLocation();
	TSharedPtr<FRootMotionSource_MoveToDynamicForce> KnockbackSource =
		MakeShared<FRootMotionSource_MoveToDynamicForce>();
	KnockbackSource->InstanceName = KnockbackRootMotionSourceName;
	KnockbackSource->Priority = 1000;
	KnockbackSource->AccumulateMode = ERootMotionAccumulateMode::Additive;
	KnockbackSource->Duration = SafeDuration;
	KnockbackSource->StartLocation = StartLocation;
	KnockbackSource->InitialTargetLocation = StartLocation + KnockbackDirection * Distance;
	KnockbackSource->TargetLocation = KnockbackSource->InitialTargetLocation;
	KnockbackSource->bRestrictSpeedToExpected = true;
	KnockbackSource->TimeMappingCurve = IsValid(KnockBackCurve) ?
        KnockBackCurve.Get() : UCurveFloat::StaticClass()->GetDefaultObject<UCurveFloat>();
	KnockbackSource->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
	KnockbackSource->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
	KnockbackSource->SetTime(SafeElapsedTime);

	ApplyRootMotionSource(KnockbackSource);
	if (CharacterOwner->HasAuthority())
	{
		CharacterOwner->ForceNetUpdate();
	}
	return true;
}

void UDRCharacterMovementComponent::SetCustomMovementMode(EDRCustomMovementMode NewMode)
{
    if (NewMode == EDRCustomMovementMode::None)
    {
        ExitCustomMovementMode();
        return;
    }
    
    /*
    * 새 이동 액션은 이전 그래플이 남긴 Falling 속도 상한을 대체한다.
    * 그래플 종료 시에는 이 함수를 거치지 않고 ExitCustomMovementMode를 사용하므로 보존 상태가 유지된다.
    */
    ClearAirborneMomentumPreservation();
    
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

void UDRCharacterMovementComponent::ReconcileMovementActionMode()
{
    if (!IsValid(CharacterOwner)
        || (!CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled()))
    {
        return;
    }

    const UDRMovementActionComponent* MovementAction = GetMovementActionComponent();
    const bool bActionActive = IsValid(MovementAction) && MovementAction->IsMovementActionActive();
    const bool bMovementModeActive = IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction);

    if (bActionActive && !bMovementModeActive)
    {
        /*
         * 네트워크 위치 보정이 CustomMode를 Falling 등으로 덮어써도
         * 유효한 ActionState가 남아 있다면 다음 프레임부터 이동 시뮬레이션을 복구한다.
         */
        SetCustomMovementMode(EDRCustomMovementMode::MovementAction);
    }
    else if (!bActionActive && bMovementModeActive)
    {
        // 반대로 액션 상태가 끝났는데 CustomMode만 남은 경우도 일반 이동으로 복귀시킨다.
        ExitCustomMovementMode();
    }    
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

bool UDRCharacterMovementComponent::TryHandleZiplineBlockingCollision(const FHitResult& Hit)
{
    /*
     * Zipline 충돌 종료는 authoritative state 변경이므로 서버만 판정한다.
     * 소유 클라이언트는 서버의 ActionState / MovementMode 복제를 받아 정리된다.
     */
    if (!IsValid(CharacterOwner) || !CharacterOwner->HasAuthority())
    {
        return false;
    }

    UDRMovementActionComponent* ThisAction = GetMovementActionComponent();

    if (!IsValid(ThisAction) || !ThisAction->IsZiplineActive())
    {
        return false;
    }

    /*
     * 상대도 Zipline 탑승자라면 기존 정책대로 양쪽을 동시에 해제한다.
     * 한쪽만 끝내면 상대는 Rail simulation을 계속하므로 서버에서 같은 시점에 종료한다.
     */
    ACharacter* OtherCharacter = Cast<ACharacter>(Hit.GetActor());

    if (IsValid(OtherCharacter) && OtherCharacter != CharacterOwner)
    {
        UDRMovementActionComponent* OtherAction =
            OtherCharacter->FindComponentByClass<UDRMovementActionComponent>();

        UDRCharacterMovementComponent* OtherMovement =
            Cast<UDRCharacterMovementComponent>(
                OtherCharacter->GetCharacterMovement());

        if (IsValid(OtherAction)
            && IsValid(OtherMovement)
            && OtherAction->IsZiplineActive())
        {
            ThisAction->EndMovementAction(
                EDRMovementActionEndReason::Collision);

            OtherAction->EndMovementAction(
                EDRMovementActionEndReason::Collision);


            /*
             * 충돌 하차에서는 Zipline 진행/Attach 보정 속도를
             * 일반 Falling으로 승계하지 않는다.
             */
            Velocity = FVector::ZeroVector;
            ZiplineRailSpeed = 0.f;

            OtherMovement->Velocity = FVector::ZeroVector;
            OtherMovement->SetZiplineRailSpeed(0.f);

            if (IsCustomMovementModeActive(
                    EDRCustomMovementMode::MovementAction))
            {
                ExitCustomMovementMode();
            }

            if (OtherMovement->IsCustomMovementModeActive(
                    EDRCustomMovementMode::MovementAction))
            {
                OtherMovement->ExitCustomMovementMode();
            }

            CharacterOwner->ForceNetUpdate();
            OtherCharacter->ForceNetUpdate();

            return true;
        }
    }

    if (Hit.GetComponent()
        && Hit.GetComponent()->IsA<UStaticMeshComponent>())
    {
        return false;
    }
    
    /*
     * Zipline 이동 중 World blocking collision이 발생하면
     * 현재 Rider만 Rail에서 해제한다.
     *
     * 눈/지형/벽 등 구체적인 Actor 타입을 CMC가 알 필요는 없다.
     * Pawn Capsule을 실제로 Block하는 장애물이면 동일한 정책을 적용한다.
     */
    ThisAction->EndMovementAction(
        EDRMovementActionEndReason::Collision);
    
    /*
     * 장애물 충돌로 떨어질 때는 기존 Zipline Velocity를 폐기한다.
     * 이후 Falling이 0 속도에서 시작하면서 중력을 적용한다.
     */
    Velocity = FVector::ZeroVector;
    ZiplineRailSpeed = 0.f;
    
    if (IsCustomMovementModeActive(
            EDRCustomMovementMode::MovementAction))
    {
        ExitCustomMovementMode();
    }

    CharacterOwner->ForceNetUpdate();

    return true;
}

void UDRCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
    switch (static_cast<EDRCustomMovementMode>(CustomMovementMode))
    {
    case EDRCustomMovementMode::VoxelContained:
        StopMovementImmediately();
        ClearAccumulatedForces();
        return;
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

void UDRCharacterMovementComponent::PhysGrabPull(
    float DeltaTime, UDRMovementActionComponent* MovementAction)
{
    const FDRMovementActionState& State = MovementAction->GetSimulationActionState();
    const FVector Destination = State.ReferenceLocation;
    const FVector StartLocation = UpdatedComponent->GetComponentLocation();
    const FVector ToDestination = Destination - StartLocation;
    const FVector GroundDelta = ProjectToGravityFloor(ToDestination);
    const float MaxDistance = FMath::Max(State.MaxSpeed, 0.f) * DeltaTime;
    FindFloor(StartLocation, CurrentFloor, false);
    const bool IsFollowingFloor = CurrentFloor.IsWalkableFloor()
        && CurrentFloor.GetDistanceToFloor() <= MAX_FLOOR_DIST
        && !GroundDelta.IsNearlyZero();
    if (IsFollowingFloor)
    {
        Velocity = GroundDelta.GetClampedToMaxSize(MaxDistance) / DeltaTime;
        MoveAlongFloor(Velocity, DeltaTime);
        if (HasValidData())
        {
            FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
            if (CurrentFloor.IsWalkableFloor())
            {
                AdjustFloorHeight();
            }
        }
    }
    else
    {
        const FVector Delta = ToDestination.GetClampedToMaxSize(MaxDistance);
        FHitResult Hit;
        SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
    }
    if (!HasValidData())
    {
        return;
    }

    const FVector Remaining = Destination - UpdatedComponent->GetComponentLocation();
    Velocity = (UpdatedComponent->GetComponentLocation() - StartLocation) / DeltaTime;
    const bool IsArrived = Remaining.IsNearlyZero()
        || (CurrentFloor.IsWalkableFloor()
            && ProjectToGravityFloor(Remaining).IsNearlyZero()
            && FMath::Abs(GetGravitySpaceZ(Remaining)) <= MAX_FLOOR_DIST);
    if (IsArrived || State.MaxSpeed <= 0.f)
    {
        StopMovementImmediately();
        if (CharacterOwner->HasAuthority())
        {
            MovementAction->EndMovementAction(IsArrived ? EDRMovementActionEndReason::Completed
                : EDRMovementActionEndReason::Invalidated);
        }
        if (!MovementAction->IsMovementActionActive()
            && IsCustomMovementModeActive(EDRCustomMovementMode::MovementAction))
        {
            RestoreDefaultMovementMode();
        }
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
    
    if (MovementAction->GetSimulationActionState().ActionType == EDRMovementActionType::Grab)
    {
        PhysGrabPull(DeltaTime, MovementAction);
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
             * 서버에서 Zipline 이동이 BlockingHit에 막히면 즉시 하차한다.
             * 상대도 Zipline Rider라면 양쪽을 동시에 하차시킨다.
             */
            if (TryHandleZiplineBlockingCollision(Hit))
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
