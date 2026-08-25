
#include "DRBreakableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include  "Net/UnrealNetwork.h"

ADRBreakableActor::ADRBreakableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	SetReplicateMovement(true);
	
	BreakableMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BreakableMeshComponent"));
	SetRootComponent(BreakableMeshComponent);
	BreakableMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	BreakableMeshComponent->SetSimulatePhysics(false);
}

void ADRBreakableActor::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentHealth = FMath::Max(1.f, MaxHealth);
	
	if (bIsBroken)
	{
		ApplyBrokenPresentation();
	}
}

void ADRBreakableActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, bIsBroken);
}

float ADRBreakableActor::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority()
		|| bIsBroken
		|| DamageAmount <= 0.f)
	{
		return 0.f;
	}
	
	const float AppliedDamage = FMath::Min(DamageAmount, CurrentHealth);
	if (AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	
	Super::TakeDamage(AppliedDamage, DamageEvent, EventInstigator, DamageCauser);
	
	CurrentHealth = FMath::Max(0.f, CurrentHealth - AppliedDamage);
	
	FDRBreakableDamageContext DamageContext;
	DamageContext.AppliedDamage = AppliedDamage;
	DamageContext.InstigatorController = EventInstigator;
	DamageContext.DamageCauser = DamageCauser;
	
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamageEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		DamageContext.HitResult = PointDamageEvent.HitInfo;
	}
	
	if (CurrentHealth <= KINDA_SMALL_NUMBER)
	{
		BreakActor(DamageContext);
	}
	
	return AppliedDamage;
}

void ADRBreakableActor::HandleBroken(const FDRBreakableDamageContext& DamageContext)
{
}

void ADRBreakableActor::OnRep_IsBroken()
{
	if (bIsBroken)
	{
		ApplyBrokenPresentation();
	}
}

void ADRBreakableActor::BreakActor(const FDRBreakableDamageContext& DamageContext)
{
	if (!HasAuthority()
		||bIsBroken)
	{
		return;
	}
	
	bIsBroken = true;
	CurrentHealth = 0.f;
	
	ApplyBrokenPresentation();
	HandleBroken(DamageContext);
	OnBroken.Broadcast(DamageContext);
	
	ForceNetUpdate();
	
	if (BrokenLifeSpan <= KINDA_SMALL_NUMBER);
	{
		Destroy();
		return;
	}
	
	SetLifeSpan(BrokenLifeSpan);
}

void ADRBreakableActor::ApplyBrokenPresentation()
{
	SetActorEnableCollision(false);
	
	if (IsValid(BreakableMeshComponent))
	{
		BreakableMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		
		if (bHideMeshWhenBroken)
		{
			BreakableMeshComponent->SetVisibility(false, true);
		}
	}
	
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnBrokenPresentation();
	}
}
