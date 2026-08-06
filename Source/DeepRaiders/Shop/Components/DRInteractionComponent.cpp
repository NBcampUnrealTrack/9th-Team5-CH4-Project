#include "DRInteractionComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UDRInteractionComponent::UDRInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	InitSphereRadius(300.f);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);
	SetHiddenInGame(true);
	ShapeColor = FColor::Green;

	OnComponentBeginOverlap.AddDynamic(
		this,
		&ThisClass::HandleBeginOverlap);
	OnComponentEndOverlap.AddDynamic(
		this,
		&ThisClass::HandleEndOverlap);
}

bool UDRInteractionComponent::IsInInteractionRange(
	const APawn* Interactor) const
{
	if (!IsValid(Interactor))
	{
		return false;
	}

	return FVector::DistSquared(
		GetComponentLocation(),
		Interactor->GetActorLocation()) <= FMath::Square(GetScaledSphereRadius());
}

void UDRInteractionComponent::Interact(APawn* Interactor)
{
	AActor* Owner = GetOwner();

	if (!IsValid(Owner) || !Owner->HasAuthority()
		|| !IsValid(Interactor))
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Interact: Interactor=%s, OverlappedObject=%s"),
		*GetNameSafe(Interactor),
		*GetNameSafe(Owner));

	OnInteracted.Broadcast(Interactor);
}

void UDRInteractionComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool IsFromSweep,
	const FHitResult& SweepResult)
{
	APawn* Interactor = Cast<APawn>(OtherActor);

	if (!IsValid(Interactor))
	{
		return;
	}

	OnInteractionEntered.Broadcast(Interactor);
	Interact(Interactor);
}

void UDRInteractionComponent::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	APawn* Interactor = Cast<APawn>(OtherActor);

	if (IsValid(Interactor))
	{
		OnInteractionExited.Broadcast(Interactor);
	}
}
