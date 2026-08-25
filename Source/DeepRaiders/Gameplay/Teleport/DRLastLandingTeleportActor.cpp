#include "DRLastLandingTeleportActor.h"

#include "Components/BoxComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

ADRLastLandingTeleportActor::ADRLastLandingTeleportActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	SetRootComponent(TriggerVolume);
	TriggerVolume->SetBoxExtent(FVector(100.f));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
}

void ADRLastLandingTeleportActor::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	ADRPlayerCharacter* PlayerCharacter = Cast<ADRPlayerCharacter>(OtherActor);
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	PlayerCharacter->TeleportTo(
		PlayerCharacter->GetLastLandedLocation(),
		PlayerCharacter->GetActorRotation(),
		false,
		true);
}
