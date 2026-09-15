#include "DRSnowWall.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ADRSnowWall::ADRSnowWall()
{
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAll"));
	CollisionComponent->SetBoxExtent(FVector(300.f, 30.f, 150.f));

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRSnowWall::ConfigureWall(const FVector& InDimensions)
{
	const FVector SafeDimensions(
		FMath::Max(1.f, InDimensions.X),
		FMath::Max(1.f, InDimensions.Y),
		FMath::Max(1.f, InDimensions.Z));

	CollisionComponent->SetBoxExtent(SafeDimensions * 0.5f);
	MeshComponent->SetRelativeScale3D(SafeDimensions / 100.f);
}
