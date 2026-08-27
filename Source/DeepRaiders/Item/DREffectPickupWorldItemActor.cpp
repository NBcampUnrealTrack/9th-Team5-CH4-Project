
#include "DREffectPickupWorldItemActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SphereComponent.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Item/Data/DRWorldItemPresentationProfile.h"
#include "DREffectPickupItemDefinition.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

ADREffectPickupWorldItemActor::ADREffectPickupWorldItemActor()
{
	InteractionSphereComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphereComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphereComponent->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandlePickupOverlap);
}

void ADREffectPickupWorldItemActor::BeginPlay()
{
	Super::BeginPlay();
	
	InteractionSphereComponent->SetSphereRadius(PickupRadius);
	RefreshPickupCollision();
	
	if (HasAuthority()
		&& UncollectedLifeSpan > 0.f)
	{
		SetLifeSpan(UncollectedLifeSpan);
	}
}

void ADREffectPickupWorldItemActor::RefreshItemPresentation()
{
	Super::RefreshItemPresentation();
	RefreshPickupCollision();
}

void ADREffectPickupWorldItemActor::HandleWorldItemStateChanged()
{
	Super::HandleWorldItemStateChanged();
	RefreshPickupCollision();
}


bool ADREffectPickupWorldItemActor::CanInteract_Implementation(APawn* Interactor) const
{
	// 상호작용이 아닌 오버랩 이벤트로만 처리
	return false;
}

bool ADREffectPickupWorldItemActor::Interact_Implementation(APawn* Interactor)
{
	// 상호작용이 아닌 오버랩 이벤트로만 처리
	return false;
}

bool ADREffectPickupWorldItemActor::GetInteractionPromptData_Implementation(APawn* Interactor,
	FDRInteractionPromptData& OutPromptData) const
{
	// 상호작용이 아닌 오버랩 이벤트로만 처리
	return false;
}

void ADREffectPickupWorldItemActor::HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()
		|| bPickupConsumed
		|| WorldItemState != EDRWorldItemState::Dropped)
	{
		return;
	}
	
	APawn* TargetPawn = Cast<APawn>(OtherActor);
	if (!IsValid(TargetPawn)
		|| !TargetPawn->IsPlayerControlled())
	{
		return;
	}
	
	// 적용 가능한 Effect가 하나라도 있어야 소비된다.
	const int32 AppliedEffectCount = ApplyPickupEffects(TargetPawn);
	if (AppliedEffectCount <= 0)
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[EffectPickup] No effect was applied. Pickup=%s Target=%s"),
			*GetName(),
			*GetNameSafe(TargetPawn));

		return;
	}
	
	bPickupConsumed = true;
	RefreshPickupCollision();
	
	MulticastPlayPickupSound(TargetPawn);
	MulticastPlayPickupPresentation();
	
	SetLifeSpan(FMath::Max(PostPickupDestroyDelay, 0.1f));
}

int32 ADREffectPickupWorldItemActor::ApplyPickupEffects(APawn* TargetPawn) const
{
	if (!HasAuthority() 
		|| !IsValid(TargetPawn))
	{
		return 0;
	}

	const UDREffectPickupItemDefinition* Definition = Cast<UDREffectPickupItemDefinition>(ItemInstance.GetDefinition());

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetPawn);

	if (!IsValid(Definition)
		|| !IsValid(TargetASC)
		|| TargetASC->HasMatchingGameplayTag(DRGameplayTags::State_Dead))
	{
		return 0;
	}

	FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();
	EffectContext.AddInstigator(TargetPawn, const_cast<ADREffectPickupWorldItemActor*>(this));
	EffectContext.AddSourceObject(Definition);

	int32 AppliedEffectCount = 0;
	
	for (const FDRGameplayEffectData& EffectData : Definition->PickupEffects)
	{
		if (!EffectData.EffectClass)
		{
			continue;
		}
		
		FGameplayEffectSpecHandle EffectSpec = TargetASC->MakeOutgoingSpec(
			EffectData.EffectClass, EffectData.EffectLevel, EffectContext);
		
		if (!EffectSpec.IsValid())
		{
			continue;
		}
		
		for (const TPair<FGameplayTag, float>& Magnitude : EffectData.SetByCallerMagnitudes)
		{
			if (Magnitude.Key.IsValid())
			{
				EffectSpec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
			}
		}
		
		const FActiveGameplayEffectHandle AppliedHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
		
		if (AppliedHandle.WasSuccessfullyApplied())
		{
			++AppliedEffectCount;
		}
	}
	
	return AppliedEffectCount;
}

void ADREffectPickupWorldItemActor::RefreshPickupCollision()
{
	if (!IsValid(InteractionSphereComponent))
	{
		return;
	}
	
	const bool bCanOverlap = HasAuthority() 
		&& !bPickupConsumed && ItemInstance.IsValid() && WorldItemState == EDRWorldItemState::Dropped;
	
	InteractionSphereComponent->SetGenerateOverlapEvents(bCanOverlap);
	InteractionSphereComponent->SetCollisionEnabled(bCanOverlap ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ADREffectPickupWorldItemActor::MulticastPlayPickupPresentation_Implementation()
{
	const UDREffectPickupItemDefinition* Definition = Cast<UDREffectPickupItemDefinition>(ItemInstance.Definition);
	const UDRWorldItemPresentationProfile* Profile = IsValid(Definition) ? Definition->WorldItemPresentationProfile.Get() : nullptr;
	
	InteractionSphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);
	
	if (GetNetMode() == NM_DedicatedServer
		|| !IsValid(Definition)
		|| !IsValid(Profile))
	{
		return;
	}
	
	const FDRWorldItemRarityVisual& RarityVisual = Profile->GetRarityVisual(Definition->Rarity);
	UNiagaraSystem* PickupBurstSystem = IsValid(RarityVisual.PickupBurstOverride) ?
		RarityVisual.PickupBurstOverride : Profile->PickupBurstSystem;
	
	if (!IsValid(PickupBurstSystem))
	{
		return;
	}
	
	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupBurstSystem,
		GetActorLocation(), GetActorRotation(), FVector::OneVector, true, false,
		ENCPoolMethod::None, true);
	
	if (!IsValid(NiagaraComponent))
	{
		return;
	}
	
	Profile->ApplyRarityParameters(NiagaraComponent, Definition->Rarity);
	NiagaraComponent->Activate(true);
}





















