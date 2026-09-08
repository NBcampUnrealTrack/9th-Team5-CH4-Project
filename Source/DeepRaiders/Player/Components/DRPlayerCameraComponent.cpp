#include "DRPlayerCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/UnrealType.h"
#include "VoxelWorld.h"

UDRPlayerCameraComponent::UDRPlayerCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	bAutoActivate = true;
	SetComponentTickEnabled(false);
}

void UDRPlayerCameraComponent::ConfigureCamera(
	USpringArmComponent* InCameraBoom,
	UCameraComponent* InFollowCamera,
	UDRCharacterMovementComponent* InMovementComponent)
{
	if (MovementComponent.IsValid() && MovementUpdatedDelegateHandle.IsValid())
	{
		MovementComponent->OnCharacterMovementUpdated.Remove(MovementUpdatedDelegateHandle);
		MovementUpdatedDelegateHandle.Reset();
	}

	CameraBoom = InCameraBoom;
	FollowCamera = InFollowCamera;
	MovementComponent = InMovementComponent;
	FirstPersonCameraAnchor = FindFirstPersonCameraAnchor();
	bVerticalFollowInitialized = false;
	bVerticalFollowActive = false;
	PerspectiveState = EDRCameraPerspectiveState::ThirdPerson;
	bFirstPersonVisualsHidden = false;
	FirstPersonBlendAlpha = 0.f;
	FirstPersonTransitionElapsed = 0.f;
	FirstPersonEnterConditionElapsed = 0.f;
	FirstPersonExitConditionElapsed = 0.f;
	bCameraDistanceInitialized = false;
	bResolvedThirdPersonCameraInitialized = false;
	CameraObstructionElapsed = 0.f;
	CameraClearPathElapsed = 0.f;
	OpenSurroundingCameraPathCount = 8;
	SurroundingCameraProbeElapsed = 0.f;
	bSurroundingCameraProbeInitialized = false;
	bCameraStateInitialized = false;
	TargetCameraState = EDRPlayerCameraState::Default;
	SetComponentTickEnabled(CameraBoom.IsValid());

	if (bEnableAutomaticFirstPerson && !FirstPersonCameraAnchor.IsValid())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[PlayerCamera] First-person anchor '%s' was not found on %s."),
			*FirstPersonCameraAnchorName.ToString(),
			*GetNameSafe(GetOwner()));
	}

	if (CameraBoom.IsValid())
	{
		ApplyCameraCollisionSettings();
		ApplyTargetLagSettings();
		CameraBoomBaseRelativeLocation = CameraBoom->GetRelativeLocation();
		DefaultThirdPersonArmLength = FMath::Max(
			DefaultThirdPersonArmLength,
			CameraBoom->TargetArmLength);
		DefaultCameraSettings.TargetArmLength = DefaultThirdPersonArmLength;
		DefaultCameraSettings.SocketOffset = CameraBoom->SocketOffset;
		CurrentCameraStateArmLength = DefaultCameraSettings.TargetArmLength;
		CurrentCameraStateSocketOffset = DefaultCameraSettings.SocketOffset;
		bCameraStateInitialized = true;
		CurrentThirdPersonArmLength = DefaultThirdPersonArmLength;
		TargetThirdPersonArmLength = DefaultThirdPersonArmLength;
		CameraBoom->TargetArmLength = CurrentThirdPersonArmLength;
		SmoothedCameraPivotZ =
			GetOwner()->GetActorLocation().Z + CameraBoomBaseRelativeLocation.Z;
		bVerticalFollowInitialized = true;
	}

	if (CameraBoom.IsValid() && FollowCamera.IsValid())
	{
		AddTickPrerequisiteComponent(CameraBoom.Get());
		if (IsLocallyControlledOwner())
		{
			ResolvedThirdPersonCameraLocation = FollowCamera->GetComponentLocation();
			ResolvedThirdPersonCameraRotation = FollowCamera->GetComponentQuat();
			bResolvedThirdPersonCameraInitialized = true;
			FollowCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}

	if (MovementComponent.IsValid())
	{
		MovementUpdatedDelegateHandle =
			MovementComponent->OnCharacterMovementUpdated.AddUObject(
				this,
				&ThisClass::HandleCharacterMovementUpdated);
	}
}

void UDRPlayerCameraComponent::UpdateCameraState(float DeltaSeconds)
{
	if (!CameraBoom.IsValid())
	{
		return;
	}

	TargetCameraState = ResolveDesiredCameraState();
	const FDRPlayerCameraStateSettings& TargetSettings = GetCameraStateSettings(TargetCameraState);
	if (!bCameraStateInitialized)
	{
		CurrentCameraStateArmLength = TargetSettings.TargetArmLength;
		CurrentCameraStateSocketOffset = TargetSettings.SocketOffset;
		bCameraStateInitialized = true;
	}
	else if (CameraStateBlendSpeed > KINDA_SMALL_NUMBER)
	{
		CurrentCameraStateArmLength = FMath::FInterpTo(
			CurrentCameraStateArmLength, TargetSettings.TargetArmLength, DeltaSeconds, CameraStateBlendSpeed);
		CurrentCameraStateSocketOffset = FMath::VInterpTo(
			CurrentCameraStateSocketOffset, TargetSettings.SocketOffset, DeltaSeconds, CameraStateBlendSpeed);
	}
	else
	{
		CurrentCameraStateArmLength = TargetSettings.TargetArmLength;
		CurrentCameraStateSocketOffset = TargetSettings.SocketOffset;
	}
}

void UDRPlayerCameraComponent::UpdateAutomaticFirstPersonView(float DeltaSeconds)
{
	if (!CameraBoom.IsValid())
	{
		return;
	}

	UpdateCameraSpace(DeltaSeconds);
	UpdatePerspectiveState(DeltaSeconds);
	UpdateFirstPersonVisualVisibility();
	DrawCameraDebug();
}

void UDRPlayerCameraComponent::UpdateCameraSpace(float DeltaSeconds)
{
	const FVector CollisionOrigin = GetCameraCollisionOrigin();
	const FTransform FullThirdPersonTransform =
		GetDesiredThirdPersonCameraTransform(CurrentCameraStateArmLength, CurrentCameraStateSocketOffset);
	FVector CentralCameraLocation = FullThirdPersonTransform.GetLocation();
	bool bCentralCameraPathBlocked = false;
	CurrentAvailableCameraDistance = EvaluateCameraSpace(
		CollisionOrigin,
		FullThirdPersonTransform,
		&CentralCameraLocation,
		&bCentralCameraPathBlocked);

	// 여러 방향의 중앙값은 1인칭 전환 판정에만 사용한다.
	// 실제 3인칭 카메라 위치는 중앙 경로의 결과만 따른다.
	RawAvailableCameraDistance = CurrentAvailableCameraDistance;
	if (!bCameraDistanceInitialized)
	{
		SmoothedAvailableCameraDistance = RawAvailableCameraDistance;
		bCameraDistanceInitialized = true;
	}
	const float FilterSpeed = RawAvailableCameraDistance < SmoothedAvailableCameraDistance
		? CameraDistanceShrinkFilterSpeed
		: CameraDistanceExpandFilterSpeed;
	SmoothedAvailableCameraDistance = FilterSpeed > KINDA_SMALL_NUMBER
		? FMath::FInterpTo(SmoothedAvailableCameraDistance, RawAvailableCameraDistance, DeltaSeconds, FilterSpeed)
		: RawAvailableCameraDistance;

	const bool bNeedsSurroundingProbe = bEnableAutomaticFirstPerson
		&& (SmoothedAvailableCameraDistance <= FirstPersonExitDistance
			|| PerspectiveState != EDRCameraPerspectiveState::ThirdPerson);
	if (bNeedsSurroundingProbe)
	{
		SurroundingCameraProbeElapsed += DeltaSeconds;
		if (!bSurroundingCameraProbeInitialized
			|| FirstPersonSurroundingProbeInterval <= KINDA_SMALL_NUMBER
			|| SurroundingCameraProbeElapsed >= FirstPersonSurroundingProbeInterval)
		{
			OpenSurroundingCameraPathCount = CountOpenSurroundingCameraPaths();
			SurroundingCameraProbeElapsed = 0.f;
			bSurroundingCameraProbeInitialized = true;
		}
	}
	else
	{
		OpenSurroundingCameraPathCount = 8;
		SurroundingCameraProbeElapsed = 0.f;
		bSurroundingCameraProbeInitialized = false;
	}

	// 이동 예측과 다중 후보가 실제 Arm 길이를 바꾸면 평범한 전진 중에도 줌이 흔들린다.
	// Spring Arm은 항상 기본 길이를 유지하고, 중앙 경로의 충돌 거리만 최종 카메라에 적용한다.
	TargetThirdPersonArmLength = CurrentCameraStateArmLength;
	CurrentThirdPersonArmLength = CurrentCameraStateArmLength;
	CameraBoom->TargetArmLength = CurrentThirdPersonArmLength;

	const FTransform& DesiredCameraTransform = FullThirdPersonTransform;
	const FVector& TargetCameraLocation = CentralCameraLocation;

	if (!bResolvedThirdPersonCameraInitialized)
	{
		ResolvedThirdPersonCameraLocation = TargetCameraLocation;
		ResolvedThirdPersonCameraRotation = DesiredCameraTransform.GetRotation();
		ResolvedThirdPersonCameraDistance = FVector::Distance(
			CollisionOrigin,
			TargetCameraLocation);
		bResolvedThirdPersonCameraInitialized = true;
	}

	const bool bCurrentPathObstructed = bCentralCameraPathBlocked;
	CameraObstructionElapsed = bCurrentPathObstructed
		? CameraObstructionElapsed + DeltaSeconds
		: 0.f;
	CameraClearPathElapsed = bCurrentPathObstructed
		? 0.f
		: CameraClearPathElapsed + DeltaSeconds;
	const bool bCanHoldPreviousLocation =
		bCurrentPathObstructed
		&& CameraObstructionElapsed < CameraObstructionConfirmTime
		&& !IsCameraLocationBlocked(ResolvedThirdPersonCameraLocation);
	if (bCanHoldPreviousLocation)
	{
		const FVector HoldDirection = (TargetCameraLocation - CollisionOrigin).GetSafeNormal();
		if (!HoldDirection.IsNearlyZero())
		{
			ResolvedThirdPersonCameraLocation = CollisionOrigin
				+ HoldDirection * ResolvedThirdPersonCameraDistance;
		}
		ResolvedThirdPersonCameraRotation = DesiredCameraTransform.GetRotation();
		return;
	}

	// 월드 위치를 보간하면 마우스 회전 궤도까지 지연되어 멀미를 유발한다.
	// 방향은 즉시 따르고, 충돌로 줄어든 거리의 복구만 별도 스칼라로 보간한다.
	const float TargetDistance = FVector::Distance(CollisionOrigin, TargetCameraLocation);
	const bool bAllowDistanceRecovery =
		CameraClearPathElapsed >= CameraCollisionRecoveryDelay;
	const float ResolvedDistance = TargetDistance < ResolvedThirdPersonCameraDistance
		? TargetDistance
		: (!bAllowDistanceRecovery
			? ResolvedThirdPersonCameraDistance
			: (CameraCollisionRecoverySpeed > KINDA_SMALL_NUMBER
			? FMath::FInterpTo(
				ResolvedThirdPersonCameraDistance,
				TargetDistance,
				DeltaSeconds,
				CameraCollisionRecoverySpeed)
			: TargetDistance));
	const FVector TargetDirection = (TargetCameraLocation - CollisionOrigin).GetSafeNormal();
	FVector SafeCandidateLocation = TargetDirection.IsNearlyZero()
		? CollisionOrigin
		: CollisionOrigin + TargetDirection * ResolvedDistance;
	SweepCameraPath(
		CollisionOrigin,
		SafeCandidateLocation,
		CameraCollisionProbeSize,
		SafeCandidateLocation);
	ResolvedThirdPersonCameraLocation = SafeCandidateLocation;
	ResolvedThirdPersonCameraRotation = DesiredCameraTransform.GetRotation();
	ResolvedThirdPersonCameraDistance = FVector::Distance(
		CollisionOrigin,
		ResolvedThirdPersonCameraLocation);
}

void UDRPlayerCameraComponent::UpdatePerspectiveState(float DeltaSeconds)
{
	if (!bEnableAutomaticFirstPerson || !FirstPersonCameraAnchor.IsValid() || !FollowCamera.IsValid())
	{
		PerspectiveState = EDRCameraPerspectiveState::ThirdPerson;
		FirstPersonBlendAlpha = 0.f;
		FirstPersonEnterConditionElapsed = 0.f;
		FirstPersonExitConditionElapsed = 0.f;
		ApplyResolvedThirdPersonCamera();
		return;
	}

	switch (PerspectiveState)
	{
	case EDRCameraPerspectiveState::ThirdPerson:
		ApplyResolvedThirdPersonCamera();
		FirstPersonEnterConditionElapsed =
			SmoothedAvailableCameraDistance <= FirstPersonEnterDistance
			&& OpenSurroundingCameraPathCount < FirstPersonMinimumOpenDirections
			? FirstPersonEnterConditionElapsed + DeltaSeconds : 0.f;
		if (FirstPersonEnterConditionElapsed >= FirstPersonEnterHoldTime)
		{
			PerspectiveState = EDRCameraPerspectiveState::EnteringFirstPerson;
			BeginFirstPersonTransition(true);
		}
		break;

	case EDRCameraPerspectiveState::EnteringFirstPerson:
	case EDRCameraPerspectiveState::ExitingFirstPerson:
	{
		const bool bEntering = PerspectiveState == EDRCameraPerspectiveState::EnteringFirstPerson;
		const float Duration = bEntering ? FirstPersonEnterBlendDuration : FirstPersonExitBlendDuration;
		FirstPersonTransitionElapsed += DeltaSeconds;
		const float LinearAlpha = Duration > KINDA_SMALL_NUMBER
			? FMath::Clamp(FirstPersonTransitionElapsed / Duration, 0.f, 1.f) : 1.f;
		const float SmoothAlpha = FMath::SmoothStep(0.f, 1.f, LinearAlpha);
		FirstPersonBlendAlpha = bEntering ? SmoothAlpha : 1.f - SmoothAlpha;
		const FTransform FirstPersonTransform = GetFirstPersonCameraTransform();
		const FTransform EndTransform = bEntering
			? FirstPersonTransform
			: FTransform(
				ResolvedThirdPersonCameraRotation,
				ResolvedThirdPersonCameraLocation);
		FollowCamera->SetWorldLocation(FMath::Lerp(TransitionStartCameraLocation, EndTransform.GetLocation(), SmoothAlpha));
		FollowCamera->SetWorldRotation(FQuat::Slerp(TransitionStartCameraRotation, EndTransform.GetRotation(), SmoothAlpha));
		if (LinearAlpha >= 1.f)
		{
			if (bEntering)
			{
				PerspectiveState = EDRCameraPerspectiveState::FirstPerson;
				FirstPersonBlendAlpha = 1.f;
			}
			else
			{
				PerspectiveState = EDRCameraPerspectiveState::ThirdPerson;
				FinishFirstPersonTransition();
			}
		}
		break;
	}

	case EDRCameraPerspectiveState::FirstPerson:
		FollowCamera->SetWorldTransform(GetFirstPersonCameraTransform());
		FirstPersonExitConditionElapsed =
			SmoothedAvailableCameraDistance >= FirstPersonExitDistance
			|| OpenSurroundingCameraPathCount >= FirstPersonMinimumOpenDirections
			? FirstPersonExitConditionElapsed + DeltaSeconds : 0.f;
		if (FirstPersonExitConditionElapsed >= FirstPersonExitHoldTime)
		{
			PerspectiveState = EDRCameraPerspectiveState::ExitingFirstPerson;
			BeginFirstPersonTransition(false);
		}
		break;
	}
}

void UDRPlayerCameraComponent::BeginFirstPersonTransition(bool bEnteringFirstPerson)
{
	if (!CameraBoom.IsValid() || !FollowCamera.IsValid()) return;
	TransitionStartCameraLocation = FollowCamera->GetComponentLocation();
	TransitionStartCameraRotation = FollowCamera->GetComponentQuat();
	FirstPersonTransitionElapsed = 0.f;
	if (FollowCamera->GetAttachParent() != nullptr)
	{
		FollowCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
	FollowCamera->bUsePawnControlRotation = false;
	if (bEnteringFirstPerson)
	{
		FirstPersonEnterConditionElapsed = 0.f;
		FirstPersonExitConditionElapsed = 0.f;
	}
	else
	{
		FirstPersonExitConditionElapsed = 0.f;
	}
}

void UDRPlayerCameraComponent::FinishFirstPersonTransition()
{
	if (!CameraBoom.IsValid() ||
		!FollowCamera.IsValid())
	{
		return;
	}

	FirstPersonBlendAlpha = 0.f;
	ApplyResolvedThirdPersonCamera();
}

void UDRPlayerCameraComponent::UpdateFirstPersonVisualVisibility()
{
	ADRPlayerCharacter* OwnerCharacter = Cast<ADRPlayerCharacter>(GetOwner());
	if (!IsValid(OwnerCharacter) || !CameraBoom.IsValid() || !FollowCamera.IsValid())
	{
		return;
	}

	const FVector CameraReference = FirstPersonCameraAnchor.IsValid()
		? FirstPersonCameraAnchor->GetComponentLocation()
		: CameraBoom->GetComponentLocation();
	const float ActualCameraDistance = FVector::Distance(FollowCamera->GetComponentLocation(), CameraReference);
	if (!bFirstPersonVisualsHidden && ActualCameraDistance <= CharacterHideDistance)
	{
		bFirstPersonVisualsHidden = true;
		OwnerCharacter->SetLocalFirstPersonVisualsHidden(true);
	}
	else if (bFirstPersonVisualsHidden && ActualCameraDistance >= CharacterShowDistance)
	{
		bFirstPersonVisualsHidden = false;
		OwnerCharacter->SetLocalFirstPersonVisualsHidden(false);
	}
}

USceneComponent* UDRPlayerCameraComponent::FindFirstPersonCameraAnchor() const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || FirstPersonCameraAnchorName.IsNone())
	{
		return nullptr;
	}

	if (const FObjectPropertyBase* AnchorProperty =
		FindFProperty<FObjectPropertyBase>(
			OwnerActor->GetClass(),
			FirstPersonCameraAnchorName))
	{
		if (USceneComponent* BlueprintAnchor = Cast<USceneComponent>(
			AnchorProperty->GetObjectPropertyValue_InContainer(OwnerActor)))
		{
			return BlueprintAnchor;
		}
	}

	TArray<USceneComponent*> SceneComponents;
	OwnerActor->GetComponents(SceneComponents);

	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (IsValid(SceneComponent) &&
			SceneComponent->GetFName() == FirstPersonCameraAnchorName)
		{
			return SceneComponent;
		}
	}

	return nullptr;
}

FTransform UDRPlayerCameraComponent::GetDesiredThirdPersonCameraTransform(
	float ArmLength,
	const FVector& SocketOffset) const
{
	if (!CameraBoom.IsValid())
	{
		return FTransform::Identity;
	}

	// Socket transform에서 Spring Arm의 위치/회전 Lag가 적용된 Arm 원점을 역산한다.
	// 충돌만 끄고 기존 추적 감각은 그대로 유지하기 위함이다.
	const FQuat ViewQuaternion =
		CameraBoom->GetSocketQuaternion(USpringArmComponent::SocketName);
	const FVector CurrentWorldSocketOffset =
		ViewQuaternion.RotateVector(CameraBoom->SocketOffset);
	const FVector DesiredWorldSocketOffset = ViewQuaternion.RotateVector(SocketOffset);
	const FVector CurrentSocketLocation =
		CameraBoom->GetSocketLocation(USpringArmComponent::SocketName);
	const FVector LaggedArmOrigin = CurrentSocketLocation
		+ ViewQuaternion.GetForwardVector() * CameraBoom->TargetArmLength
		- CurrentWorldSocketOffset;

	// Spring Arm은 이 컴포넌트가 데드존 보정을 적용하기 전에 Tick된다.
	// 따라서 위치 랙으로 역산한 Arm 원점의 Z를 그대로 쓰면, 작은 지형 높이
	// 변화가 다음 프레임의 카메라 위치로 다시 유입되어 데드존을 우회한다.
	// 수평 위치 랙은 유지하되, 수직 기준점은 보정이 끝난 Boom 피벗으로 고정한다.
	FVector ArmOrigin = LaggedArmOrigin;
	ArmOrigin.Z = CameraBoom->GetComponentLocation().Z;
	const FVector CameraLocation =
		ArmOrigin - ViewQuaternion.GetForwardVector() * FMath::Max(0.f, ArmLength)
		+ DesiredWorldSocketOffset;
	return FTransform(ViewQuaternion, CameraLocation);
}

float UDRPlayerCameraComponent::EvaluateCameraSpace(
	const FVector& TraceStart,
	const FTransform& DesiredCameraTransform,
	FVector* OutCentralCameraLocation,
	bool* OutCentralPathBlocked) const
{
	const FVector DesiredLocation = DesiredCameraTransform.GetLocation();
	if (!bEnableCameraCollision)
	{
		if (OutCentralCameraLocation)
		{
			*OutCentralCameraLocation = DesiredLocation;
		}
		if (OutCentralPathBlocked)
		{
			*OutCentralPathBlocked = false;
		}
		return FVector::Distance(TraceStart, DesiredLocation);
	}

	const FQuat CameraRotation = DesiredCameraTransform.GetRotation();
	const FVector Right = CameraRotation.GetRightVector();
	const FVector BaseCameraDelta = DesiredLocation - TraceStart;
	const float YawStepRadians = FMath::DegreesToRadians(
		FMath::Max(0.f, CameraAvoidanceYawStepDegrees));
	const float MaxYawRadians = FMath::DegreesToRadians(
		FMath::Max(CameraAvoidanceYawStepDegrees, CameraAvoidanceMaxYawDegrees));
	const float PitchRadians = FMath::DegreesToRadians(
		FMath::Max(0.f, CameraAvoidancePitchDegrees));

	TArray<FVector, TInlineAllocator<6>> CandidateEnds;
	CandidateEnds.Add(DesiredLocation);
	CandidateEnds.Add(TraceStart + FQuat(FVector::UpVector, YawStepRadians).RotateVector(BaseCameraDelta));
	CandidateEnds.Add(TraceStart + FQuat(FVector::UpVector, -YawStepRadians).RotateVector(BaseCameraDelta));
	CandidateEnds.Add(TraceStart + FQuat(FVector::UpVector, MaxYawRadians).RotateVector(BaseCameraDelta));
	CandidateEnds.Add(TraceStart + FQuat(FVector::UpVector, -MaxYawRadians).RotateVector(BaseCameraDelta));
	const FVector PitchPositive = FQuat(Right, PitchRadians).RotateVector(BaseCameraDelta);
	const FVector PitchNegative = FQuat(Right, -PitchRadians).RotateVector(BaseCameraDelta);
	CandidateEnds.Add(TraceStart + (
		PitchPositive.Z >= PitchNegative.Z ? PitchPositive : PitchNegative));

	TArray<float, TInlineAllocator<6>> AvailableDistances;
	AvailableDistances.Reserve(CandidateEnds.Num());
	TArray<FVector, TInlineAllocator<6>> SafeLocations;
	SafeLocations.Reserve(CandidateEnds.Num());
	TArray<bool, TInlineAllocator<6>> BlockedPaths;
	BlockedPaths.Reserve(CandidateEnds.Num());

	for (int32 CandidateIndex = 0; CandidateIndex < CandidateEnds.Num(); ++CandidateIndex)
	{
		const FVector& CandidateEnd = CandidateEnds[CandidateIndex];
		FVector SafeLocation = CandidateEnd;
		const bool bBlocked = SweepCameraPath(
			TraceStart,
			CandidateEnd,
			CameraCollisionProbeSize,
			SafeLocation);
		const float AvailableDistance = FVector::Distance(TraceStart, SafeLocation);
		AvailableDistances.Add(AvailableDistance);
		SafeLocations.Add(SafeLocation);
		BlockedPaths.Add(bBlocked);
	}

	AvailableDistances.Sort();
	const int32 MiddleIndex = AvailableDistances.Num() / 2;
	const float MedianAvailableDistance = AvailableDistances.IsValidIndex(MiddleIndex)
		? AvailableDistances[MiddleIndex]
		: FVector::Distance(TraceStart, DesiredLocation);

	if (OutCentralCameraLocation && SafeLocations.Num() > 0)
	{
		*OutCentralCameraLocation = SafeLocations[0];
	}
	if (OutCentralPathBlocked && BlockedPaths.Num() > 0)
	{
		*OutCentralPathBlocked = BlockedPaths[0];
	}
	return MedianAvailableDistance;
}

bool UDRPlayerCameraComponent::SweepCameraPath(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	float ProbeRadius,
	FVector& OutSafeLocation) const
{
	OutSafeLocation = TraceEnd;
	UWorld* World = GetWorld();
	if (!bEnableCameraCollision || !IsValid(World))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(DRPlayerCameraCollision),
		false,
		GetOwner());
	FHitResult HitResult;
	const bool bBlockingHit = ProbeRadius > KINDA_SMALL_NUMBER
		? World->SweepSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			CameraCollisionProbeChannel,
			FCollisionShape::MakeSphere(ProbeRadius),
			QueryParams)
		: World->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			CameraCollisionProbeChannel,
			QueryParams);

	if (!bBlockingHit)
	{
		return false;
	}

	const FVector TraceDelta = TraceEnd - TraceStart;
	const float HitDistance = HitResult.bStartPenetrating
		? 0.f
		: FVector::Distance(TraceStart, HitResult.Location);
	const float SafeDistance = FMath::Max(0.f, HitDistance - CameraCollisionMargin);
	OutSafeLocation = TraceDelta.IsNearlyZero()
		? TraceStart
		: TraceStart + TraceDelta.GetSafeNormal() * SafeDistance;
	return true;
}

bool UDRPlayerCameraComponent::IsCameraLocationBlocked(
	const FVector& CameraLocation) const
{
	UWorld* World = GetWorld();
	if (!bEnableCameraCollision || !IsValid(World))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(DRPlayerCameraOverlap),
		false,
		GetOwner());
	return World->OverlapBlockingTestByChannel(
		CameraLocation,
		FQuat::Identity,
		CameraCollisionProbeChannel,
		FCollisionShape::MakeSphere(FMath::Max(0.f, CameraCollisionProbeSize)),
		QueryParams);
}

FVector UDRPlayerCameraComponent::GetCameraCollisionOrigin() const
{
	// 3인칭 충돌은 Spring Arm 피벗을 기준으로 해야 이동 랙/데드존과 같은 좌표계를 쓴다.
	// 별도 1인칭 앵커를 사용하면 두 컴포넌트의 미세한 이동 차이가 거리 흔들림으로 증폭된다.
	if (CameraBoom.IsValid())
	{
		return CameraBoom->GetComponentLocation();
	}
	return FirstPersonCameraAnchor.IsValid()
		? FirstPersonCameraAnchor->GetComponentLocation()
		: GetOwner()->GetActorLocation();
}

int32 UDRPlayerCameraComponent::CountOpenSurroundingCameraPaths() const
{
	const FVector TraceStart = GetCameraCollisionOrigin();
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AController* Controller = IsValid(OwnerPawn)
		? OwnerPawn->GetController()
		: nullptr;
	const float BaseYaw = IsValid(Controller)
		? Controller->GetControlRotation().Yaw
		: GetOwner()->GetActorRotation().Yaw;
	const float ProbeDistance = FMath::Max(0.f, FirstPersonSurroundingProbeDistance);
	const float RequiredOpenDistance = FMath::Min(
		FMath::Max(0.f, FirstPersonSurroundingOpenDistance),
		ProbeDistance);

	int32 OpenPathCount = 0;
	for (int32 DirectionIndex = 0; DirectionIndex < 8; ++DirectionIndex)
	{
		const float DirectionYaw = BaseYaw + DirectionIndex * 45.f;
		const FVector Direction = FRotator(0.f, DirectionYaw, 0.f).Vector();
		const FVector TraceEnd = TraceStart + Direction * ProbeDistance;
		FVector SafeLocation = TraceEnd;
		SweepCameraPath(
			TraceStart,
			TraceEnd,
			CameraCollisionProbeSize,
			SafeLocation);
		if (FVector::Distance(TraceStart, SafeLocation) >= RequiredOpenDistance)
		{
			++OpenPathCount;
		}
	}
	return OpenPathCount;
}

void UDRPlayerCameraComponent::ApplyResolvedThirdPersonCamera()
{
	if (!FollowCamera.IsValid() || !bResolvedThirdPersonCameraInitialized)
	{
		return;
	}
	if (FollowCamera->GetAttachParent() != nullptr)
	{
		FollowCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
	FollowCamera->SetWorldLocation(ResolvedThirdPersonCameraLocation);
	FollowCamera->SetWorldRotation(ResolvedThirdPersonCameraRotation);
}

FTransform UDRPlayerCameraComponent::GetFirstPersonCameraTransform() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AController* Controller = IsValid(OwnerPawn) ? OwnerPawn->GetController() : nullptr;
	return FTransform(
		IsValid(Controller)
			? Controller->GetControlRotation().Quaternion()
			: FirstPersonCameraAnchor->GetComponentQuat(),
		FirstPersonCameraAnchor->GetComponentLocation());
}

void UDRPlayerCameraComponent::DrawCameraDebug() const
{
	if (!bDrawCameraDebug || !GEngine)
	{
		return;
	}

	static const TCHAR* StateNames[] = {
		TEXT("ThirdPerson"), TEXT("EnteringFirstPerson"),
		TEXT("FirstPerson"), TEXT("ExitingFirstPerson") };
	const int32 StateIndex = static_cast<int32>(PerspectiveState);
	GEngine->AddOnScreenDebugMessage(
		reinterpret_cast<uint64>(this),
		0.f,
		FColor::Cyan,
		FString::Printf(
			TEXT("Camera Median: %.1f Filtered: %.1f Arm: %.1f Resolved: %.1f Clear: %.2f Open: %d State: %s"),
			CurrentAvailableCameraDistance,
			SmoothedAvailableCameraDistance,
			CurrentThirdPersonArmLength,
			ResolvedThirdPersonCameraDistance,
			CameraClearPathElapsed,
			OpenSurroundingCameraPathCount,
			StateNames[FMath::Clamp(StateIndex, 0, UE_ARRAY_COUNT(StateNames) - 1)]));
}

void UDRPlayerCameraComponent::ApplyCameraCollisionSettings() const
{
	if (!CameraBoom.IsValid())
	{
		return;
	}

	// 기본 Spring Arm 충돌은 한 프레임 Hit에도 Socket을 즉시 당겼다가 놓는다.
	// 최종 카메라 충돌과 복귀는 이 컴포넌트가 일관되게 처리한다.
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->ProbeSize = CameraCollisionProbeSize;
	CameraBoom->ProbeChannel = CameraCollisionProbeChannel;

	if (!bEnableCameraCollision || !bForceVoxelWorldCameraBlocking)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		It->SetCollisionResponseToChannel(
			CameraCollisionProbeChannel,
			ECR_Block);
	}
}

void UDRPlayerCameraComponent::ApplyTargetLagSettings() const
{
	if (!CameraBoom.IsValid())
	{
		return;
	}

	CameraBoom->bEnableCameraLag = bEnableTargetPositionLag;
	CameraBoom->CameraLagSpeed = TargetPositionLagSpeed;
	CameraBoom->CameraLagMaxDistance = TargetPositionLagMaxDistance;
	CameraBoom->bEnableCameraRotationLag = bEnableTargetRotationLag;
	CameraBoom->CameraRotationLagSpeed = TargetRotationLagSpeed;
	CameraBoom->bUseCameraLagSubstepping = bUseTargetLagSubstepping;
}

UCameraShakeBase* UDRPlayerCameraComponent::PlayCameraShake(
	TSubclassOf<UCameraShakeBase> ShakeClass,
	float Scale) const
{
	if (!ShakeClass || !IsLocallyControlledOwner())
	{
		return nullptr;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	APlayerController* PlayerController =
		OwnerCharacter ? Cast<APlayerController>(OwnerCharacter->GetController()) : nullptr;

	return IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager)
		? PlayerController->PlayerCameraManager->StartCameraShake(
			ShakeClass,
			Scale,
			ECameraShakePlaySpace::CameraLocal,
			FRotator::ZeroRotator)
		: nullptr;
}

void UDRPlayerCameraComponent::StopCameraShake(
	UCameraShakeBase* ShakeInstance,
	bool bImmediately) const
{
	if (!IsValid(ShakeInstance) || !IsLocallyControlledOwner())
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	APlayerController* PlayerController =
		OwnerCharacter ? Cast<APlayerController>(OwnerCharacter->GetController()) : nullptr;

	if (IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager))
	{
		PlayerController->PlayerCameraManager->StopCameraShake(ShakeInstance, bImmediately);
	}
}

void UDRPlayerCameraComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsLocallyControlledOwner() || !CameraBoom.IsValid())
	{
		return;
	}

	bMovementUpdatedSinceLastTick = false;
	UpdateCameraState(DeltaTime);

	const UDRCharacterMovementComponent* CharacterMovement = MovementComponent.Get();
	if (!IsValid(CharacterMovement) || !CharacterMovement->IsMovingOnGround())
	{
		// 점프, 낙하, 제트팩 중의 높이 변화는 의도된 이동이므로
		// Z축 데드존 보정 없이 Spring Arm의 타겟 랙으로만 따라가게 한다.
		SmoothedCameraPivotZ =
			GetOwner()->GetActorLocation().Z + CameraBoomBaseRelativeLocation.Z;
		bVerticalFollowActive = false;
		ApplyCameraBoomLocation();
	}
	else
	{
		UpdateVerticalFollow(true, DeltaTime);
	}

	// 수직 피벗을 먼저 확정한 뒤 현재/예측 공간과 시점 상태를 갱신한다.
	UpdateAutomaticFirstPersonView(DeltaTime);

	// 카메라 회전만으로도 Sweep 결과가 달라질 수 있으므로 Tick을 유지한다.
}

void UDRPlayerCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CameraBoom.IsValid())
	{
		RemoveTickPrerequisiteComponent(CameraBoom.Get());
	}

	if (MovementComponent.IsValid() && MovementUpdatedDelegateHandle.IsValid())
	{
		MovementComponent->OnCharacterMovementUpdated.Remove(MovementUpdatedDelegateHandle);
		MovementUpdatedDelegateHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void UDRPlayerCameraComponent::HandleCharacterMovementUpdated(
	float DeltaSeconds,
	const FVector& OldLocation,
	const FVector& OldVelocity)
{
	if (!IsLocallyControlledOwner() || !CameraBoom.IsValid())
	{
		return;
	}

	const bool bLocationChanged = !GetOwner()->GetActorLocation().Equals(OldLocation, 0.01f);
	const bool bVelocityChanged =
		!MovementComponent->Velocity.Equals(OldVelocity, 0.01f);

	if (bLocationChanged || bVelocityChanged)
	{
		// 이동 계산의 중간 단계에서는 Spring Arm을 갱신하지 않는다.
		// 카메라 Tick이 물리 이동 이후 한 번만 위치를 반영한다.
		bMovementUpdatedSinceLastTick = true;
		SetComponentTickEnabled(true);
	}
}

void UDRPlayerCameraComponent::UpdateVerticalFollow(
	bool bAllowInterpolation,
	float DeltaSeconds)
{
	const float TargetPivotZ =
		GetOwner()->GetActorLocation().Z + CameraBoomBaseRelativeLocation.Z;

	if (!bVerticalFollowInitialized)
	{
		SmoothedCameraPivotZ = TargetPivotZ;
		bVerticalFollowInitialized = true;
	}

	const float VerticalDifference = TargetPivotZ - SmoothedCameraPivotZ;
	const float ReleaseDeadZone = FMath::Min(
		VerticalFollowReleaseDeadZone,
		VerticalFollowDeadZone);
	if (VerticalFollowSnapDistance > KINDA_SMALL_NUMBER &&
		FMath::Abs(VerticalDifference) >= VerticalFollowSnapDistance)
	{
		SmoothedCameraPivotZ = TargetPivotZ;
		bVerticalFollowActive = false;
	}
	else if (!bVerticalFollowActive)
	{
		if (FMath::Abs(VerticalDifference) > VerticalFollowDeadZone)
		{
			bVerticalFollowActive = true;
		}
	}

	if (bVerticalFollowActive && bAllowInterpolation)
	{
		SmoothedCameraPivotZ = VerticalFollowSpeed > KINDA_SMALL_NUMBER
			? FMath::FInterpTo(
				SmoothedCameraPivotZ,
				TargetPivotZ,
				DeltaSeconds,
				VerticalFollowSpeed)
			: TargetPivotZ;

		if (FMath::Abs(TargetPivotZ - SmoothedCameraPivotZ)
			<= ReleaseDeadZone)
		{
			bVerticalFollowActive = false;
		}
	}

	ApplyCameraBoomLocation();
}

void UDRPlayerCameraComponent::ApplyCameraBoomLocation() const
{
	if (!CameraBoom.IsValid())
	{
		return;
	}

	FVector BoomRelativeLocation = CameraBoomBaseRelativeLocation;
	BoomRelativeLocation.Z = SmoothedCameraPivotZ - GetOwner()->GetActorLocation().Z;
	CameraBoom->SetRelativeLocation(BoomRelativeLocation);
}

EDRPlayerCameraState UDRPlayerCameraComponent::ResolveDesiredCameraState() const
{
	const ADRPlayerCharacter* OwnerCharacter = Cast<ADRPlayerCharacter>(GetOwner());
	const UAbilitySystemComponent* AbilitySystem = IsValid(OwnerCharacter)
		? OwnerCharacter->GetAbilitySystemComponent()
		: nullptr;
	return IsValid(AbilitySystem)
		&& AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Aiming_Throw)
		? EDRPlayerCameraState::ThrowAim
		: EDRPlayerCameraState::Default;
}

const FDRPlayerCameraStateSettings& UDRPlayerCameraComponent::GetCameraStateSettings(
	EDRPlayerCameraState CameraState) const
{
	return CameraState == EDRPlayerCameraState::ThrowAim
		? ThrowAimCameraSettings
		: DefaultCameraSettings;
}

bool UDRPlayerCameraComponent::IsLocallyControlledOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return IsValid(OwnerPawn) && OwnerPawn->IsLocallyControlled();
}
