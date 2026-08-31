#include "DRPlayerCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

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
	SetComponentTickEnabled(false);

	if (CameraBoom.IsValid())
	{
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

	UpdateVerticalFollow(true, DeltaTime);
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

	// Tick이 꺼진 상태에서도 지형의 미세한 Z 이동을 상쇄해야 한다.
	UpdateVerticalFollow(false, DeltaSeconds);
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
	if (FMath::Abs(VerticalDifference) >= VerticalFollowSnapDistance)
	{
		SmoothedCameraPivotZ = TargetPivotZ;
		SetComponentTickEnabled(false);
	}
	else if (FMath::Abs(VerticalDifference) > VerticalFollowDeadZone)
	{
		if (bAllowInterpolation)
		{
			const float DesiredPivotZ = TargetPivotZ -
				FMath::Sign(VerticalDifference) * VerticalFollowDeadZone;

			SmoothedCameraPivotZ = VerticalFollowSpeed > KINDA_SMALL_NUMBER
				? FMath::FInterpTo(
					SmoothedCameraPivotZ,
					DesiredPivotZ,
					DeltaSeconds,
					VerticalFollowSpeed)
				: DesiredPivotZ;

			if (FMath::IsNearlyEqual(
				SmoothedCameraPivotZ,
				DesiredPivotZ,
				VerticalFollowStopTolerance))
			{
				SetComponentTickEnabled(false);
			}
		}
		else
		{
			SetComponentTickEnabled(true);
		}
	}
	else
	{
		SetComponentTickEnabled(false);
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
