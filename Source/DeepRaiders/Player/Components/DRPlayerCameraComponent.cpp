#include "DRPlayerCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
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
	bVerticalFollowInitialized = false;
	bVerticalFollowActive = false;
	SetComponentTickEnabled(false);

	if (CameraBoom.IsValid())
	{
		ApplyCameraCollisionSettings();
		ApplyTargetLagSettings();
		CameraBoomBaseRelativeLocation = CameraBoom->GetRelativeLocation();
		SmoothedCameraPivotZ =
			GetOwner()->GetActorLocation().Z + CameraBoomBaseRelativeLocation.Z;
		bVerticalFollowInitialized = true;
	}

	if (MovementComponent.IsValid())
	{
		MovementUpdatedDelegateHandle =
			MovementComponent->OnCharacterMovementUpdated.AddUObject(
				this,
				&ThisClass::HandleCharacterMovementUpdated);
	}
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
		SetComponentTickEnabled(false);
		return;
	}

	const bool bMovementUpdatedThisFrame = bMovementUpdatedSinceLastTick;
	bMovementUpdatedSinceLastTick = false;

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

	if (!bMovementUpdatedThisFrame &&
		!bVerticalFollowActive &&
		bCharacterIsStationary)
	{
		SetComponentTickEnabled(false);
	}
}

void UDRPlayerCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
