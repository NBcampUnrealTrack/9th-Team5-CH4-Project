#include "DRInstantCareProjectile.h"

#include "AbilitySystemComponent.h"
#include "Components/DrawSphereComponent.h"
#include "DrawDebugHelpers.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

ADRInstantCareProjectile::ADRInstantCareProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	ExplosionRadiusGizmo = CreateEditorOnlyDefaultSubobject<UDrawSphereComponent>(TEXT("ExplosionRadiusGizmo"));
	if (IsValid(ExplosionRadiusGizmo))
	{
		ExplosionRadiusGizmo->SetupAttachment(GetRootComponent());
		ExplosionRadiusGizmo->ShapeColor = FColor::Green;
		ExplosionRadiusGizmo->SetHiddenInGame(false);
	}
#endif
}

bool ADRInstantCareProjectile::IsValidEffectTarget(const AActor* TargetActor) const
{
	return IsValid(TargetActor) && IsFriendlyTarget(TargetActor);
}

void ADRInstantCareProjectile::BeginPlay()
{
	Super::BeginPlay();

#if WITH_EDITORONLY_DATA
	if (HasAuthority() && IsValid(ExplosionRadiusGizmo))
	{
		ExplosionRadiusGizmo->SetSphereRadius(GetExplosionRadius());
		ExplosionRadiusGizmo->SetVisibility(true);
	}
#endif
}

void ADRInstantCareProjectile::HandleImpact(const FHitResult& ImpactResult)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[InstantCare][Impact] Projectile=%s HitActor=%s Location=%s Radius=%.1f"),
		*GetNameSafe(this),
		*GetNameSafe(ImpactResult.GetActor()),
		*ImpactResult.ImpactPoint.ToCompactString(),
		GetExplosionRadius());

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(
		GetWorld(),
		ImpactResult.ImpactPoint,
		GetExplosionRadius(),
		32,
		FColor::Green,
		false,
		5.0f,
		0,
		2.0f);
#endif

	Super::HandleImpact(ImpactResult);
}

void ADRInstantCareProjectile::HandleTargetRejected(
	const AActor* TargetActor,
	EDRThrowableTargetRejectReason RejectReason) const
{
	const TCHAR* Reason = TEXT("RejectedByPolicy");
	switch (RejectReason)
	{
	case EDRThrowableTargetRejectReason::InvalidTeam:
		Reason = TEXT("NotFriendly");
		break;
	case EDRThrowableTargetRejectReason::Occluded:
		Reason = TEXT("Occluded");
		break;
	case EDRThrowableTargetRejectReason::MissingAbilitySystem:
		Reason = TEXT("MissingASC");
		break;
	default:
		break;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[InstantCare][TargetSkipped] Target=%s Reason=%s"),
		*GetNameSafe(TargetActor),
		Reason);
}

void ADRInstantCareProjectile::ApplyEffectToTarget(
	AActor* TargetActor,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FHitResult& ImpactResult)
{
	if (TargetAbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Dead))
	{
		UE_LOG(LogTemp, Log, TEXT("[InstantCare][TargetSkipped] Target=%s Reason=Dead"),
			*GetNameSafe(TargetActor));
		return;
	}

	const float HealthBefore = TargetAbilitySystem->GetNumericAttribute(
		UDRPlayerAttributeSet::GetHealthAttribute());
	const bool IsFrozenBefore = TargetAbilitySystem->HasMatchingGameplayTag(
		DRGameplayTags::State_Frozen);

	ApplyImpactEffect(TargetAbilitySystem, ImpactResult);

	ADRPlayerState* TargetPlayerState = Cast<ADRPlayerState>(TargetAbilitySystem->GetOwnerActor());
	if (IsValid(TargetPlayerState))
	{
		TargetPlayerState->ClearFrozenState();
	}

	const float HealthAfter = TargetAbilitySystem->GetNumericAttribute(UDRPlayerAttributeSet::GetHealthAttribute());
	const bool IsFrozenAfter = TargetAbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Frozen);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[InstantCare][TargetApplied] Target=%s Health=%.1f->%.1f Frozen=%s->%s"),
		*GetNameSafe(TargetActor),
		HealthBefore,
		HealthAfter,
		IsFrozenBefore ? TEXT("True") : TEXT("False"),
		IsFrozenAfter ? TEXT("True") : TEXT("False"));
}

void ADRInstantCareProjectile::HandleWorldImpact(const FHitResult& /*ImpactResult*/)
{
}
