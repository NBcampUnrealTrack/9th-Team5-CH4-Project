#include "DRZiplineEndpoint.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/Player/Components/DRCharacterMovementComponent.h"
#include "DeepRaiders/Player/Components/DRMovementActionComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "GameFramework/Pawn.h"

ADRZiplineEndpoint::ADRZiplineEndpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(Root);
	InteractionVolume->SetBoxExtent(FVector(60.f, 60.f, 60.f));
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionVolume->SetCollisionResponseToChannel(DRCollisionChannels::Interaction, ECR_Overlap);
}

bool ADRZiplineEndpoint::CanInteract_Implementation(APawn* Interactor) const
{
	return CanStartZiplineRide(Interactor);
}

bool ADRZiplineEndpoint::Interact_Implementation(APawn* Interactor)
{
	if (!HasAuthority()
		|| !CanStartZiplineRide(Interactor))
	{
		return false;
	}

	ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(Interactor);

	if (!IsValid(Character))
	{
		return false;
	}

	UDRMovementActionComponent* MovementAction = Character->GetMovementActionComponent();

	UDRCharacterMovementComponent* Movement = Cast<UDRCharacterMovementComponent>(Character->GetCharacterMovement());

	if (!IsValid(MovementAction)
		|| !IsValid(Movement))
	{
		return false;
	}

	// 서버-예측 세션을 구분할 필요가 없는 서버 전용 시작이므로, 0이 아닌 값만 보장하면 된다.
	static int32 NextZiplineSessionId = 1;

	FDRMovementActionState State;
	State.bActive = true;
	State.ActionType = EDRMovementActionType::Zipline;
	State.SessionId = NextZiplineSessionId++;
	State.ReferenceLocation = LinkedEndpoint->GetActorLocation();
	State.MaxSpeed = ZiplineSpeed;
	State.ZiplineStartLocation = GetActorLocation();
	State.ZiplineRideMode = RideMode;
	State.ZiplineStartRideOffset = RideOffset;
	State.ZiplineTargetRideOffset = LinkedEndpoint->RideOffset;

	if (!MovementAction->StartAuthoritativeMovementAction(State))
	{
		return false;
	}

	Movement->SetCustomMovementMode(EDRCustomMovementMode::MovementAction);

	return true;
}

bool ADRZiplineEndpoint::CanStartZiplineRide(APawn* Interactor) const
{
	if (!IsValid(LinkedEndpoint)
		|| LinkedEndpoint == this
		|| ZiplineSpeed <= 0.f)
	{
		return false;
	}

	if (FVector::DistSquared(GetActorLocation(), LinkedEndpoint->GetActorLocation())
		<= FMath::Square(MinZiplineDistance))
	{
		return false;
	}

	const ADRPlayerCharacter* Character = Cast<ADRPlayerCharacter>(Interactor);

	if (!IsValid(Character))
	{
		return false;
	}

	const UDRMovementActionComponent* MovementAction = Character->GetMovementActionComponent();

	if (!IsValid(MovementAction)
		|| MovementAction->IsMovementActionActive())
	{
		return false;
	}

	return true;
}
