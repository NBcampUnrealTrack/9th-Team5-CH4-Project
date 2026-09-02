#include "DRGrabProjectile.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"

ADRGrabProjectile::ADRGrabProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ADRGrabProjectile::InitializeGrabProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	int32 InSourceTeamId,
	float InMaxDistance,
	float InPullSpeed,
	float InPullDestinationDistance)
{
	LaunchLocation = GetActorLocation();
	MaxDistance = FMath::Max(InMaxDistance, 0.f);
	PullSpeed = FMath::Max(InPullSpeed, 0.f);
	PullDestinationDistance = FMath::Max(InPullDestinationDistance, 0.f);
	SetActorTickEnabled(MaxDistance > 0.f);

	InitializeProjectile(
		InSourceAbilitySystem,
		TArray<FGameplayEffectSpecHandle>(),
		0.f,
		FDRProjectileWorldImpactData(),
		InSourceTeamId,
		nullptr);
}

void ADRGrabProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority()
		&& FVector::DistSquared(LaunchLocation, GetActorLocation()) >= FMath::Square(MaxDistance))
	{
		Destroy();
	}
}

void ADRGrabProjectile::HandleImpact(const FHitResult& ImpactResult)
{
	PullTarget(ImpactResult);
	Destroy();
}

void ADRGrabProjectile::PullTarget(const FHitResult& ImpactResult) const
{
	ADRPlayerCharacter* SourceCharacter = Cast<ADRPlayerCharacter>(GetInstigator());
	ADRPlayerCharacter* TargetCharacter = Cast<ADRPlayerCharacter>(ImpactResult.GetActor());

	if (!HasAuthority()
		|| !IsValid(SourceCharacter)
		|| !IsValid(TargetCharacter)
		|| IsFriendlyTarget(TargetCharacter)
		|| PullSpeed <= 0.f)
	{
		return;
	}

	const FVector PullDestination =
		SourceCharacter->GetActorLocation()
		+ SourceCharacter->GetActorForwardVector().GetSafeNormal2D() * PullDestinationDistance;
	const FVector PullDirection =
		(PullDestination - TargetCharacter->GetActorLocation()).GetSafeNormal();

	if (PullDirection.IsNearlyZero())
	{
		return;
	}

	TargetCharacter->LaunchCharacter(
		PullDirection * PullSpeed,
		true,
		true);
}
