#include "DRShopAreaComponent.h"

#include "GameFramework/Pawn.h"

UDRShopAreaComponent::UDRShopAreaComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	InitBoxExtent(FVector(300.f));
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

void UDRShopAreaComponent::BeginPlay()
{
	Super::BeginPlay();

	SetHiddenInGame(!IsDebugVisible);
}

void UDRShopAreaComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool IsFromSweep,
	const FHitResult& SweepResult)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (!OverlappingPawns.Contains(Pawn))
		{
			OverlappingPawns.Add(Pawn);
			OnPawnEntered.Broadcast(Pawn);
		}
	}
}

void UDRShopAreaComponent::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (!IsOverlappingActor(Pawn))
		{
			OverlappingPawns.Remove(Pawn);
			OnPawnExited.Broadcast(Pawn);
		}
	}
}
