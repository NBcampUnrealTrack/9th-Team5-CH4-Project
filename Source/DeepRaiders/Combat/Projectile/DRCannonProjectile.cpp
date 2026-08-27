#include "DRCannonProjectile.h"

#include "Components/BoxComponent.h"

ADRCannonProjectile::ADRCannonProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBoxComponent>(CollisionComponentName))
{
	UBoxComponent* BoxCollision = CastChecked<UBoxComponent>(CollisionComponent);

	BoxCollision->InitBoxExtent(FVector(40.f, 12.f, 12.f));
}

void ADRCannonProjectile::HandleImpact(const FHitResult& ImpactResult)
{
	// 아직 AoE 구현 전.
	// 우선 기존 Projectile처럼 동작하는지만 테스트.
	Super::HandleImpact(ImpactResult);
}
