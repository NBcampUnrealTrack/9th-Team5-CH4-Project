#include "DRPlayerCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
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
	bFirstPersonTransitionActive = false;
	bFirstPersonVisualsHidden = false;
	FirstPersonBlendAlpha = 0.f;
	SetComponentTickEnabled(FirstPersonCameraAnchor.IsValid());

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
		SmoothedCameraPivotZ =
			GetOwner()->GetActorLocation().Z + CameraBoomBaseRelativeLocation.Z;
		bVerticalFollowInitialized = true;
	}

	if (CameraBoom.IsValid() && FollowCamera.IsValid())
	{
		AddTickPrerequisiteComponent(CameraBoom.Get());
	}

	if (MovementComponent.IsValid())
	{
		MovementUpdatedDelegateHandle =
			MovementComponent->OnCharacterMovementUpdated.AddUObject(
				this,
				&ThisClass::HandleCharacterMovementUpdated);
	}
}

void UDRPlayerCameraComponent::UpdateAutomaticFirstPersonView(float DeltaSeconds)
{
	if (!bEnableAutomaticFirstPerson ||
		!FirstPersonCameraAnchor.IsValid() ||
		!CameraBoom.IsValid() ||
		!FollowCamera.IsValid())
	{
		return;
	}

	const float AvailableCameraDistance =
		GetAvailableThirdPersonCameraDistance();
	const float FullThirdPersonDistance = FMath::Max(
		FirstPersonExitDistance,
		FirstPersonEnterDistance);
	const float DistanceRange =
		FullThirdPersonDistance - FirstPersonEnterDistance;
	const float TargetBlendAlpha = DistanceRange > KINDA_SMALL_NUMBER
		? 1.f - FMath::Clamp(
			(AvailableCameraDistance - FirstPersonEnterDistance) / DistanceRange,
			0.f,
			1.f)
		: (AvailableCameraDistance <= FirstPersonEnterDistance ? 1.f : 0.f);

	if (TargetBlendAlpha > KINDA_SMALL_NUMBER &&
		!bFirstPersonTransitionActive)
	{
		BeginFirstPersonTransition();
	}

	if (!bFirstPersonTransitionActive)
	{
		return;
	}

	FirstPersonBlendAlpha = FirstPersonBlendSpeed > KINDA_SMALL_NUMBER
		? FMath::FInterpTo(
			FirstPersonBlendAlpha,
			TargetBlendAlpha,
			DeltaSeconds,
			FirstPersonBlendSpeed)
		: TargetBlendAlpha;

	const FVector ThirdPersonLocation =
		CameraBoom->GetSocketLocation(USpringArmComponent::SocketName);
	const FQuat ThirdPersonRotation =
		CameraBoom->GetSocketQuaternion(USpringArmComponent::SocketName);
	const FVector FirstPersonLocation =
		FirstPersonCameraAnchor->GetComponentLocation();

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AController* Controller = IsValid(OwnerPawn)
		? OwnerPawn->GetController()
		: nullptr;
	const FQuat FirstPersonRotation = IsValid(Controller)
		? Controller->GetControlRotation().Quaternion()
		: FirstPersonCameraAnchor->GetComponentQuat();

	FollowCamera->SetWorldLocation(FMath::Lerp(
		ThirdPersonLocation,
		FirstPersonLocation,
		FirstPersonBlendAlpha));
	FollowCamera->SetWorldRotation(FQuat::Slerp(
		ThirdPersonRotation,
		FirstPersonRotation,
		FirstPersonBlendAlpha));

	UpdateFirstPersonVisualVisibility();

	if (TargetBlendAlpha <= KINDA_SMALL_NUMBER &&
		FirstPersonBlendAlpha <= 0.01f)
	{
		FinishFirstPersonTransition();
	}
}

void UDRPlayerCameraComponent::BeginFirstPersonTransition()
{
	if (bFirstPersonTransitionActive ||
		!CameraBoom.IsValid() ||
		!FollowCamera.IsValid())
	{
		return;
	}

	bFirstPersonTransitionActive = true;
	FollowCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	FollowCamera->bUsePawnControlRotation = false;

	// 전환 중에는 캐릭터 기준 위치와 회전을 지연 없이 사용한다.
	SmoothedCameraPivotZ =
		GetOwner()->GetActorLocation().Z + CameraBoomBaseRelativeLocation.Z;
	bVerticalFollowInitialized = true;
	bVerticalFollowActive = false;
	ApplyCameraBoomLocation();
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
}

void UDRPlayerCameraComponent::FinishFirstPersonTransition()
{
	if (!bFirstPersonTransitionActive ||
		!CameraBoom.IsValid() ||
		!FollowCamera.IsValid())
	{
		return;
	}

	FirstPersonBlendAlpha = 0.f;
	bFirstPersonTransitionActive = false;

	ADRPlayerCharacter* OwnerCharacter = Cast<ADRPlayerCharacter>(GetOwner());
	if (IsValid(OwnerCharacter) && bFirstPersonVisualsHidden)
	{
		OwnerCharacter->SetLocalFirstPersonVisualsHidden(false);
	}
	bFirstPersonVisualsHidden = false;

	FollowCamera->AttachToComponent(
		CameraBoom.Get(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		USpringArmComponent::SocketName);

	SmoothedCameraPivotZ =
		GetOwner()->GetActorLocation().Z + CameraBoomBaseRelativeLocation.Z;
	bVerticalFollowInitialized = true;
	bVerticalFollowActive = false;
	ApplyCameraBoomLocation();
	ApplyTargetLagSettings();
}

void UDRPlayerCameraComponent::UpdateFirstPersonVisualVisibility()
{
	ADRPlayerCharacter* OwnerCharacter = Cast<ADRPlayerCharacter>(GetOwner());
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	const float HideAlpha = FMath::Clamp(FirstPersonHideVisualAlpha, 0.f, 1.f);
	const float ShowAlpha = FMath::Min(
		FMath::Clamp(FirstPersonShowVisualAlpha, 0.f, 1.f),
		HideAlpha);

	if (!bFirstPersonVisualsHidden && FirstPersonBlendAlpha >= HideAlpha)
	{
		bFirstPersonVisualsHidden = true;
		OwnerCharacter->SetLocalFirstPersonVisualsHidden(true);
	}
	else if (bFirstPersonVisualsHidden && FirstPersonBlendAlpha <= ShowAlpha)
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

float UDRPlayerCameraComponent::GetAvailableThirdPersonCameraDistance() const
{
	if (!CameraBoom.IsValid() ||
		!FirstPersonCameraAnchor.IsValid() ||
		!bEnableCameraCollision)
	{
		return TNumericLimits<float>::Max();
	}

	UWorld* World = GetWorld();
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(World) || !IsValid(OwnerPawn))
	{
		return TNumericLimits<float>::Max();
	}

	const AController* Controller = OwnerPawn->GetController();
	const FRotator ViewRotation = IsValid(Controller)
		? Controller->GetControlRotation()
		: CameraBoom->GetComponentRotation();
	const FVector TraceStart = FirstPersonCameraAnchor->GetComponentLocation();
	const FVector TraceEnd =
		TraceStart - ViewRotation.Vector() * CameraBoom->TargetArmLength
		+ FRotationMatrix(ViewRotation).TransformVector(CameraBoom->SocketOffset);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(DRAutomaticFirstPerson),
		false,
		GetOwner());
	FHitResult HitResult;
	const bool bBlockingHit = FirstPersonTransitionProbeSize > KINDA_SMALL_NUMBER
		? World->SweepSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			CameraCollisionProbeChannel,
			FCollisionShape::MakeSphere(FirstPersonTransitionProbeSize),
			QueryParams)
		: World->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			CameraCollisionProbeChannel,
			QueryParams);

	return bBlockingHit
		? FVector::Distance(TraceStart, HitResult.Location)
		: FVector::Distance(TraceStart, TraceEnd);
}

void UDRPlayerCameraComponent::ApplyCameraCollisionSettings() const
{
	if (!CameraBoom.IsValid())
	{
		return;
	}

	CameraBoom->bDoCollisionTest = bEnableCameraCollision;
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
		if (!FirstPersonCameraAnchor.IsValid())
		{
			SetComponentTickEnabled(false);
		}
		return;
	}

	UpdateAutomaticFirstPersonView(DeltaTime);

	const bool bMovementUpdatedThisFrame = bMovementUpdatedSinceLastTick;
	bMovementUpdatedSinceLastTick = false;

	if (bFirstPersonTransitionActive)
	{
		return;
	}

	const UDRCharacterMovementComponent* CharacterMovement = MovementComponent.Get();
	if (!IsValid(CharacterMovement) || !CharacterMovement->IsMovingOnGround())
	{
		// 점프, 낙하, 제트팩 중의 높이 변화는 의도된 이동이므로
		// Z축 데드존 보정 없이 Spring Arm의 타겟 랙으로만 따라가게 한다.
		SmoothedCameraPivotZ =
			GetOwner()->GetActorLocation().Z + CameraBoomBaseRelativeLocation.Z;
		bVerticalFollowActive = false;
		ApplyCameraBoomLocation();
		return;
	}

	UpdateVerticalFollow(true, DeltaTime);

	const bool bCharacterIsStationary =
		CharacterMovement->Velocity.IsNearlyZero(0.1f);

	if (!FirstPersonCameraAnchor.IsValid() &&
		!bMovementUpdatedThisFrame &&
		!bVerticalFollowActive &&
		bCharacterIsStationary)
	{
		SetComponentTickEnabled(false);
	}
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

bool UDRPlayerCameraComponent::IsLocallyControlledOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return IsValid(OwnerPawn) && OwnerPawn->IsLocallyControlled();
}
