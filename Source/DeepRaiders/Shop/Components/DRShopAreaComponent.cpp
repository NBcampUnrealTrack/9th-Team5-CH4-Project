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
		int32& OverlapCount = PawnOverlapCounts.FindOrAdd(Pawn);
		++OverlapCount;

		if (OverlapCount == 1)
		{
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
		int32* OverlapCount = PawnOverlapCounts.Find(Pawn);

		if (!OverlapCount)
		{
			return;
		}

		--(*OverlapCount);

		if (*OverlapCount <= 0)
		{
			PawnOverlapCounts.Remove(Pawn);
			OnPawnExited.Broadcast(Pawn);
		}
	}
}
