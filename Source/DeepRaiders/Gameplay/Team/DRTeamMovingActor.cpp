#include "DRTeamMovingActor.h"

#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ADRTeamMovingActor::ADRTeamMovingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ADRTeamMovingActor::BeginPlay()
{
	Super::BeginPlay();

	if (!bLocationInitialized)
	{
		InactiveLocation = GetActorLocation();
		bLocationInitialized = true;
	}

	RefreshTeamColor();
}

void ADRTeamMovingActor::RefreshTeamColor()
{
	const FLinearColor TeamColor = TeamId == 0 ? Team0Color : Team1Color;
	TArray<UMeshComponent*> MeshComponents;
	GetComponents(MeshComponents);

	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		for (int32 MaterialIndex = 0; MaterialIndex < MeshComponent->GetNumMaterials(); ++MaterialIndex)
		{
			UMaterialInstanceDynamic* Material =
				MeshComponent->CreateDynamicMaterialInstance(MaterialIndex);
			if (IsValid(Material))
			{
				Material->SetVectorParameterValue(TeamColorParameterName, TeamColor);
			}
		}
	}
}

void ADRTeamMovingActor::SetTeamActive(bool bActive, bool bImmediate)
{
	if (!HasAuthority())
	{
		return;
	}
	if (!bLocationInitialized)
	{
		InactiveLocation = GetActorLocation();
		bLocationInitialized = true;
	}

	MoveStartLocation = GetActorLocation();
	MoveTargetLocation = InactiveLocation +
		(bActive ? GetActorRotation().RotateVector(MoveOffset) : FVector::ZeroVector);
	MoveElapsedTime = 0.f;

	if (bImmediate || MoveDuration <= 0.f)
	{
		SetActorLocation(MoveTargetLocation);
		bMoving = false;
		SetActorTickEnabled(false);
		return;
	}

	bMoving = true;
	SetActorTickEnabled(true);
}

void ADRTeamMovingActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bMoving)
	{
		return;
	}

	MoveElapsedTime += DeltaSeconds;
	const float Alpha = FMath::Clamp(MoveElapsedTime / MoveDuration, 0.f, 1.f);
	SetActorLocation(FMath::Lerp(MoveStartLocation, MoveTargetLocation, Alpha));

	if (Alpha >= 1.f)
	{
		bMoving = false;
		SetActorTickEnabled(false);
	}
}
