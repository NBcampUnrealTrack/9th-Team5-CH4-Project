#include "DRZiplineRope.h"

#include "DRZiplineEndpoint.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"

ADRZiplineRope::ADRZiplineRope()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("Root"));

	SetRootComponent(Root);

	InteractionVolume =
		CreateDefaultSubobject<UBoxComponent>(
			TEXT("InteractionVolume"));

	InteractionVolume->SetupAttachment(Root);
	InteractionVolume->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	InteractionVolume->SetCollisionResponseToAllChannels(
		ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(
		DRCollisionChannels::Interaction,
		ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(true);

	CableVisual =
		CreateDefaultSubobject<USplineMeshComponent>(
			TEXT("CableVisual"));

	CableVisual->SetupAttachment(Root);
	CableVisual->SetMobility(EComponentMobility::Movable);
	CableVisual->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	CableVisual->SetGenerateOverlapEvents(false);
	CableVisual->SetForwardAxis(
		ESplineMeshAxis::X,
		false);
}

void ADRZiplineRope::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshRopeGeometry();
}

void ADRZiplineRope::BeginPlay()
{
	Super::BeginPlay();

	RefreshRopeGeometry();

	// Runtime에서는 Endpoint를 정적 레벨 설정으로 취급한다.
	SetActorTickEnabled(false);
}

void ADRZiplineRope::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if WITH_EDITOR
	if (GetWorld() != nullptr
		&& !GetWorld()->IsGameWorld())
	{
		RefreshRopeGeometry();
	}
#endif
}

bool ADRZiplineRope::CanInteract_Implementation(
	APawn* Interactor) const
{
	return CanStartZiplineRide(Interactor);
}

bool ADRZiplineRope::Interact_Implementation(
	APawn* Interactor)
{
	if (!HasAuthority()
		|| !CanStartZiplineRide(Interactor))
	{
		return false;
	}

	ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(Interactor);

	if (!IsValid(Character))
	{
		return false;
	}

	UDRMovementActionComponent* MovementAction =
		Character->GetMovementActionComponent();

	UDRCharacterMovementComponent* Movement =
		Cast<UDRCharacterMovementComponent>(
			Character->GetCharacterMovement());

	if (!IsValid(MovementAction)
		|| !IsValid(Movement))
	{
		return false;
	}

	FVector LocationA;
	FVector LocationB;

	if (!ResolveEndpointLocations(
			LocationA,
			LocationB))
	{
		return false;
	}

	FVector StateStart = LocationA;
	FVector StateTarget = LocationB;

	if (RideMode == EDRZiplineRideMode::AutoTraverse)
	{
		const bool bTargetEndpointB =
			ShouldAutoTargetEndpointB(
				Character,
				LocationA,
				LocationB);

		if (!bTargetEndpointB)
		{
			StateStart = LocationB;
			StateTarget = LocationA;
		}
	}

	static int32 NextZiplineSessionId = 1;

	if (NextZiplineSessionId == 0)
	{
		++NextZiplineSessionId;
	}

	FDRMovementActionState State;
	State.bActive = true;
	State.ActionType = EDRMovementActionType::Zipline;
	State.SessionId = NextZiplineSessionId++;
	State.ZiplineStartLocation = StateStart;
	State.ReferenceLocation = StateTarget;

	/*
	 * RideOffset은 Character SkeletalMesh presentation용이다.
	 * Gameplay Capsule은 실제 Cable A-B rail을 그대로 따라간다.
	 */
	State.ZiplineStartRideOffset = RideOffset;
	State.ZiplineTargetRideOffset = RideOffset;
	State.ZiplineRideMode = RideMode;
	State.ZiplineManualControlMode =
		ManualControlMode;
	State.MaxSpeed = MaxSpeed;

	/*
	 * Animation / presentation용 몸 방향은 탑승 순간 서버에서 한 번 확정해
	 * FDRMovementActionState와 함께 복제한다.
	 *
	 * Auto       : 선택된 진행 Endpoint 방향
	 * Manual Vert: 높은 Endpoint 방향의 수평 성분 (완전 수직이면 현재 몸 방향)
	 * Manual View: 탑승 순간 카메라가 바라보던 Rope 방향
	 */
	FVector FacingDirection =
		Character->GetActorForwardVector()
		.GetSafeNormal2D();

	if (RideMode == EDRZiplineRideMode::AutoTraverse)
	{
		const FVector AutoFacing =
			(
				State.GetZiplineRideTargetLocation()
				- State.GetZiplineRideStartLocation()
			)
			.GetSafeNormal2D();

		if (!AutoFacing.IsNearlyZero())
		{
			FacingDirection = AutoFacing;
		}
	}
	else
	{
		FVector ManualFacing =
			State.GetZiplineManualPositiveAxis();

		if (ManualControlMode
			== EDRZiplineManualControlMode::ViewRelative)
		{
			FVector ViewDirection =
				Character->GetActorForwardVector()
				.GetSafeNormal();

			if (const AController* Controller =
					Character->GetController())
			{
				FVector ViewLocation;
				FRotator ViewRotation;

				Controller->GetPlayerViewPoint(
					ViewLocation,
					ViewRotation);

				const FVector ControllerViewDirection =
					ViewRotation.Vector()
					.GetSafeNormal();

				if (!ControllerViewDirection.IsNearlyZero())
				{
					ViewDirection = ControllerViewDirection;
				}
			}

			float ViewDot =
				FVector::DotProduct(
					ViewDirection,
					ManualFacing);

			if (FMath::Abs(ViewDot) < 0.1f)
			{
				ViewDot =
					FVector::DotProduct(
						Character->GetActorForwardVector(),
						ManualFacing);
			}

			if (ViewDot < 0.f)
			{
				ManualFacing *= -1.f;
			}
		}

		ManualFacing.Z = 0.f;
		ManualFacing = ManualFacing.GetSafeNormal();

		if (!ManualFacing.IsNearlyZero())
		{
			FacingDirection = ManualFacing;
		}
	}

	if (FacingDirection.IsNearlyZero())
	{
		FacingDirection = FVector::ForwardVector;
	}

	State.ZiplineFacingDirection =
		FacingDirection.GetSafeNormal();

	// Facing은 이동/animation presentation에 사용하며 gameplay rail은 고정이다.
	if (RideMode == EDRZiplineRideMode::AutoTraverse)
	{
		State.ZiplineAcceleration =
			AutoAcceleration;

		const FVector TravelAxis =
			(
				State.GetZiplineRideTargetLocation()
				- State.GetZiplineRideStartLocation()
			)
			.GetSafeNormal();

		const float SafeEntrySpeedCap =
			FMath::Min(
				FMath::Max(
					AutoMaxEntrySpeed,
					0.f),
				MaxSpeed);

		State.ZiplineInitialSpeed =
			TravelAxis.IsNearlyZero()
				? 0.f
				: FMath::Clamp(
					FVector::DotProduct(
						Movement->Velocity,
						TravelAxis),
					0.f,
					SafeEntrySpeedCap);
	}
	else
	{
		State.ZiplineAcceleration =
			ManualAcceleration;

		State.ZiplineBrakingDeceleration =
			ManualBrakingDeceleration;

		State.ZiplineInitialSpeed = 0.f;
	}

	if (!MovementAction->StartAuthoritativeMovementAction(
			State))
	{
		return false;
	}

	/*
	 * 서버의 첫 custom-physics tick부터
	 * 명확한 초기 조건을 사용한다.
	 */
	if (RideMode == EDRZiplineRideMode::ManualTraverse)
	{
		Movement->SetZiplineRailSpeed(0.f);
		Movement->ResetManualZiplineInputState();
		Movement->Velocity = FVector::ZeroVector;
	}
	else
	{
		const FVector TravelAxis =
			(
				State.GetZiplineRideTargetLocation()
				- State.GetZiplineRideStartLocation()
			)
			.GetSafeNormal();

		Movement->SetZiplineRailSpeed(
			State.ZiplineInitialSpeed);

		Movement->Velocity =
			TravelAxis
			* State.ZiplineInitialSpeed;
	}

	Movement->SetCustomMovementMode(
		EDRCustomMovementMode::MovementAction);

	return true;
}

bool ADRZiplineRope::GetInteractionPromptData_Implementation(
	APawn* Interactor,
	FDRInteractionPromptData& OutPromptData) const
{
	OutPromptData.ActionText =
		NSLOCTEXT(
			"DRInteraction",
			"ZiplineRideAction",
			"탑승");

	OutPromptData.TitleText =
		NSLOCTEXT(
			"DRInteraction",
			"ZiplineTitle",
			"짚라인");

	OutPromptData.DetailText =
		FText::GetEmpty();

	return true;
}

bool ADRZiplineRope::GetInteractionLocation_Implementation(
	APawn* Interactor,
	FVector& OutInteractionLocation) const
{
	FVector LocationA;
	FVector LocationB;

	if (!ResolveEndpointLocations(
			LocationA,
			LocationB)
		|| !IsValid(Interactor))
	{
		return false;
	}

	const AController* Controller =
		Interactor->GetController();

	if (IsValid(Controller))
	{
		FVector ViewLocation;
		FRotator ViewRotation;

		Controller->GetPlayerViewPoint(
			ViewLocation,
			ViewRotation);

		const FVector ViewDirection =
			ViewRotation.Vector()
			.GetSafeNormal();

		if (!ViewDirection.IsNearlyZero())
		{
			OutInteractionLocation =
				ClosestPointOnSegmentToViewRay(
					LocationA,
					LocationB,
					ViewLocation,
					ViewDirection);

			return !OutInteractionLocation.ContainsNaN();
		}
	}

	OutInteractionLocation =
		FMath::ClosestPointOnSegment(
			Interactor->GetActorLocation(),
			LocationA,
			LocationB);

	return !OutInteractionLocation.ContainsNaN();
}

bool ADRZiplineRope::CanStartZiplineRide(
	APawn* Interactor) const
{
	if (MaxSpeed <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FVector LocationA;
	FVector LocationB;

	if (!ResolveEndpointLocations(
			LocationA,
			LocationB))
	{
		return false;
	}

	if (FVector::DistSquared(
			LocationA,
			LocationB)
		<= FMath::Square(
			MinRopeLength))
	{
		return false;
	}

	const ADRPlayerCharacter* Character =
		Cast<ADRPlayerCharacter>(Interactor);

	if (!IsValid(Character))
	{
		return false;
	}

	const UDRMovementActionComponent* MovementAction =
		Character->GetMovementActionComponent();

	return IsValid(MovementAction)
		&& !MovementAction->IsMovementActionActive();
}

bool ADRZiplineRope::ResolveEndpointLocations(
	FVector& OutEndpointA,
	FVector& OutEndpointB) const
{
	OutEndpointA = FVector::ZeroVector;
	OutEndpointB = FVector::ZeroVector;

	if (!IsValid(EndpointA)
		|| !IsValid(EndpointB)
		|| EndpointA == EndpointB)
	{
		return false;
	}

	OutEndpointA =
		EndpointA->GetActorLocation();

	OutEndpointB =
		EndpointB->GetActorLocation();

	return !OutEndpointA.ContainsNaN()
		&& !OutEndpointB.ContainsNaN();
}

bool ADRZiplineRope::ShouldAutoTargetEndpointB(
	const ADRPlayerCharacter* Character,
	const FVector& LocationA,
	const FVector& LocationB) const
{
	if (!IsValid(Character))
	{
		return true;
	}

	const FVector MountPoint =
		FMath::ClosestPointOnSegment(
			Character->GetActorLocation(),
			LocationA,
			LocationB);

	const float EndpointSnapDistanceSquared =
		FMath::Square(
			AutoEndpointSnapDistance);

	/*
	 * Endpoint 바로 옆에서 잡은 경우에는
	 * 항상 반대편으로 출발시켜 기존 Endpoint UX를 보존한다.
	 */
	if (FVector::DistSquared(
			MountPoint,
			LocationA)
		<= EndpointSnapDistanceSquared)
	{
		return true;
	}

	if (FVector::DistSquared(
			MountPoint,
			LocationB)
		<= EndpointSnapDistanceSquared)
	{
		return false;
	}

	FVector ViewDirection =
		Character->GetActorForwardVector()
		.GetSafeNormal();

	if (const AController* Controller =
			Character->GetController())
	{
		FVector ViewLocation;
		FRotator ViewRotation;

		Controller->GetPlayerViewPoint(
			ViewLocation,
			ViewRotation);

		const FVector ControllerViewDirection =
			ViewRotation.Vector()
			.GetSafeNormal();

		if (!ControllerViewDirection.IsNearlyZero())
		{
			ViewDirection =
				ControllerViewDirection;
		}
	}

	const FVector ToA =
		(LocationA - MountPoint)
		.GetSafeNormal();

	const FVector ToB =
		(LocationB - MountPoint)
		.GetSafeNormal();

	const float DotA =
		FVector::DotProduct(
			ViewDirection,
			ToA);

	const float DotB =
		FVector::DotProduct(
			ViewDirection,
			ToB);

	if (!FMath::IsNearlyEqual(
			DotA,
			DotB,
			0.01f))
	{
		return DotB > DotA;
	}

	/*
	 * Rope에 거의 수직으로 시선을 둔 애매한 경우에는
	 * Rope A->B 축에 대한 시선 projection으로 결정한다.
	 */
	const FVector RopeAxis =
		(LocationB - LocationA)
		.GetSafeNormal();

	return FVector::DotProduct(
			ViewDirection,
			RopeAxis) >= 0.f;
}

void ADRZiplineRope::RefreshRopeGeometry()
{
	FVector LocationA;
	FVector LocationB;

	if (!ResolveEndpointLocations(
			LocationA,
			LocationB))
	{
		if (IsValid(InteractionVolume))
		{
			InteractionVolume->SetCollisionEnabled(
				ECollisionEnabled::NoCollision);
		}

		if (IsValid(CableVisual))
		{
			CableVisual->SetVisibility(false);
		}

		return;
	}

	if (LocationA.Equals(
			CachedEndpointA,
			0.1f)
		&& LocationB.Equals(
			CachedEndpointB,
			0.1f))
	{
		return;
	}

	CachedEndpointA = LocationA;
	CachedEndpointB = LocationB;

	const FVector Segment =
		LocationB - LocationA;

	const float SegmentLength =
		Segment.Size();

	if (SegmentLength <= KINDA_SMALL_NUMBER)
	{
		InteractionVolume->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);

		CableVisual->SetVisibility(false);
		return;
	}

	const FVector SegmentDirection =
		Segment / SegmentLength;

	const FVector MidPoint =
		(LocationA + LocationB) * 0.5f;

	InteractionVolume->SetWorldLocationAndRotation(
		MidPoint,
		SegmentDirection.Rotation());

	InteractionVolume->SetBoxExtent(
		FVector(
			SegmentLength * 0.5f
				+ FMath::Max(
					InteractionEndPadding,
					0.f),
			FMath::Max(
				InteractionHalfWidth,
				1.f),
			FMath::Max(
				InteractionHalfWidth,
				1.f)));

	InteractionVolume->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly);

	const FTransform ActorTransform =
		GetActorTransform();

	const FVector LocalStart =
		ActorTransform.InverseTransformPosition(
			LocationA);

	const FVector LocalEnd =
		ActorTransform.InverseTransformPosition(
			LocationB);

	const FVector LocalTangent =
		LocalEnd - LocalStart;

	CableVisual->SetStaticMesh(CableMesh);
	CableVisual->SetForwardAxis(
		ESplineMeshAxis::X,
		false);

	CableVisual->SetStartAndEnd(
		LocalStart,
		LocalTangent,
		LocalEnd,
		LocalTangent,
		false);

	const FVector2D SafeCableScale(
		FMath::Max(
			CableScale.X,
			0.01f),
		FMath::Max(
			CableScale.Y,
			0.01f));

	CableVisual->SetStartScale(
		SafeCableScale,
		false);

	CableVisual->SetEndScale(
		SafeCableScale,
		false);

	CableVisual->SetMaterial(
		0,
		CableMaterialOverride);

	CableVisual->SetVisibility(
		IsValid(CableMesh),
		true);

	CableVisual->MarkRenderStateDirty();
}

FVector ADRZiplineRope::ClosestPointOnSegmentToViewRay(
	const FVector& SegmentStart,
	const FVector& SegmentEnd,
	const FVector& ViewLocation,
	const FVector& ViewDirection)
{
	const FVector Segment =
		SegmentEnd - SegmentStart;

	const float SegmentLengthSquared =
		Segment.SizeSquared();

	const FVector RayDirection =
		ViewDirection.GetSafeNormal();

	if (SegmentLengthSquared <= KINDA_SMALL_NUMBER
		|| RayDirection.IsNearlyZero())
	{
		return SegmentStart;
	}

	/*
	 * Segment P(s) = A + sU, 0<=s<=1
	 * Ray     R(t) = V + tD, t>=0
	 *
	 * 먼저 두 무한 직선의 최근접 해를 구하고,
	 * segment / ray 범위로 clamp한 뒤 한 번 재투영한다.
	 */
	const FVector W =
		SegmentStart - ViewLocation;

	const float B =
		FVector::DotProduct(
			Segment,
			RayDirection);

	const float D =
		FVector::DotProduct(
			Segment,
			W);

	const float E =
		FVector::DotProduct(
			RayDirection,
			W);

	const float Denominator =
		SegmentLengthSquared - B * B;

	float SegmentAlpha = 0.f;

	if (FMath::Abs(Denominator)
		> KINDA_SMALL_NUMBER)
	{
		SegmentAlpha =
			(B * E - D)
			/ Denominator;
	}
	else
	{
		// 거의 평행한 경우 ViewLocation에 가장 가까운 segment 위치부터 시작.
		SegmentAlpha =
			-D / SegmentLengthSquared;
	}

	SegmentAlpha =
		FMath::Clamp(
			SegmentAlpha,
			0.f,
			1.f);

	FVector SegmentPoint =
		SegmentStart
		+ Segment
		* SegmentAlpha;

	const float RayDistance =
		FMath::Max(
			0.f,
			FVector::DotProduct(
				SegmentPoint - ViewLocation,
				RayDirection));

	const FVector RayPoint =
		ViewLocation
		+ RayDirection
		* RayDistance;

	SegmentAlpha =
		FMath::Clamp(
			FVector::DotProduct(
				RayPoint - SegmentStart,
				Segment)
			/ SegmentLengthSquared,
			0.f,
			1.f);

	return SegmentStart
		+ Segment
		* SegmentAlpha;
}
