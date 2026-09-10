
#include "DRBreakableActor.h"

#include "Components/StaticMeshComponent.h"
#include "DeepRaiders/GAS/Cues/DRGameplayCuePresentationLibrary.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"

ADRBreakableActor::ADRBreakableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	SetReplicateMovement(true);
	
	BreakableMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BreakableMeshComponent"));
	SetRootComponent(BreakableMeshComponent);
	BreakableMeshComponent->SetCollisionProfileName(TEXT("DRBreakable"));
	BreakableMeshComponent->SetCollisionObjectType(DRCollisionChannels::Breakable);
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

float ADRBreakableActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
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
	
	const bool bDestroyedThisHit = CurrentHealth <= KINDA_SMALL_NUMBER;
	if (HitSoundCueTag.IsValid() || (bDestroyedThisHit && DestroyedSoundCueTag.IsValid()))
	{
		const FVector SoundLocation = DamageContext.HitResult.bBlockingHit
			? DamageContext.HitResult.ImpactPoint
			: GetActorLocation();
		APawn* InstigatorPawn = IsValid(EventInstigator) ? EventInstigator->GetPawn().Get() : Cast<APawn>(DamageCauser);

		MulticastPlayDamageSoundFeedback(SoundLocation, InstigatorPawn, bDestroyedThisHit);
	}

	if (bDestroyedThisHit)
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

void ADRBreakableActor::MulticastPlayDamageSoundFeedback_Implementation(
	FVector_NetQuantize SoundLocation,
	APawn* InstigatorPawn,
	bool bDestroyedThisHit)
{
	FGameplayCueParameters Parameters;
	Parameters.Location = SoundLocation;
	Parameters.Instigator = InstigatorPawn;
	Parameters.EffectCauser = this;

	if (HitSoundCueTag.IsValid())
	{
		UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(this, HitSoundCueTag, Parameters);
	}

	if (bDestroyedThisHit && DestroyedSoundCueTag.IsValid())
	{
		UDRGameplayCuePresentationLibrary::ExecuteLocalSoundCue(this, DestroyedSoundCueTag, Parameters);
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
	
	if (!ShouldDeferBrokenDestruction())
	{
		StartBrokenDestructionCountdown();
	}
}

bool ADRBreakableActor::ShouldDeferBrokenDestruction() const
{
	return false;
}

void ADRBreakableActor::StartBrokenDestructionCountdown()
{
	if (!HasAuthority() || !bIsBroken)
	{
		return;
	}

	if (BrokenLifeSpan <= KINDA_SMALL_NUMBER)
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
