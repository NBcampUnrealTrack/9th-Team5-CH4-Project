#include "DRHotPackArea.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

#include "DeepRaiders/Combat/Team/DRCombatTeamLibrary.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"

ADRHotPackArea::ADRHotPackArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RecoveryArea = CreateDefaultSubobject<USphereComponent>(TEXT("RecoveryArea"));
	RecoveryArea->SetupAttachment(SceneRoot);
	RecoveryArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RecoveryArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	RecoveryArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RecoveryArea->SetGenerateOverlapEvents(true);

	HotPackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HotPackMesh"));
	HotPackMesh->SetupAttachment(SceneRoot);
	HotPackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RecoveryArea->OnComponentBeginOverlap.AddDynamic(
		this,
		&ThisClass::HandleBeginOverlap);
	RecoveryArea->OnComponentEndOverlap.AddDynamic(
		this,
		&ThisClass::HandleEndOverlap);
}

void ADRHotPackArea::Initialize(ADRPlayerCharacter* SourceCharacter)
{
	if (!HasAuthority() || !IsValid(SourceCharacter))
	{
		return;
	}

	SourceAbilitySystem = SourceCharacter->GetAbilitySystemComponent();
	SourceTeamId = DRCombatTeam::GetActorTeamId(SourceCharacter);
}

void ADRHotPackArea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RecoveryArea->SetSphereRadius(AreaRadius);
}

void ADRHotPackArea::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		RecoveryArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	SetLifeSpan(AreaDuration);

	TArray<AActor*> OverlappingActors;
	RecoveryArea->GetOverlappingActors(OverlappingActors, ADRPlayerCharacter::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		ApplyRecovery(OverlappingActor);
	}
}

void ADRHotPackArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllRecoveries();
	Super::EndPlay(EndPlayReason);
}

void ADRHotPackArea::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool IsFromSweep,
	const FHitResult& SweepResult)
{
	ApplyRecovery(OtherActor);
}

void ADRHotPackArea::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	if (!RecoveryArea->IsOverlappingActor(OtherActor))
	{
		RemoveRecovery(OtherActor);
	}
}

void ADRHotPackArea::ApplyRecovery(AActor* TargetActor)
{
	if (!HasAuthority()
		|| !IsValid(TargetActor)
		|| !SourceAbilitySystem.IsValid()
		|| SourceTeamId == INDEX_NONE
		|| !RecoveryEffectClass
		|| !DRCombatTeam::IsFriendlyTarget(SourceTeamId, TargetActor))
	{
		return;
	}

	UAbilitySystemComponent* TargetAbilitySystem = GetTargetAbilitySystem(TargetActor);
	if (!IsValid(TargetAbilitySystem)
		|| ActiveRecoveryEffects.Contains(TargetAbilitySystem)
		|| TargetAbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Dead)
		|| TargetAbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Frozen))
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle EffectSpec = SourceAbilitySystem->MakeOutgoingSpec(
		RecoveryEffectClass,
		1.0f,
		EffectContext);
	if (!EffectSpec.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle EffectHandle =
		SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
			*EffectSpec.Data.Get(),
			TargetAbilitySystem);
	if (EffectHandle.IsValid())
	{
		ActiveRecoveryEffects.Add(TargetAbilitySystem, EffectHandle);
	}
}

void ADRHotPackArea::RemoveRecovery(AActor* TargetActor)
{
	UAbilitySystemComponent* TargetAbilitySystem = GetTargetAbilitySystem(TargetActor);
	if (!IsValid(TargetAbilitySystem))
	{
		return;
	}

	const FActiveGameplayEffectHandle* EffectHandle =
		ActiveRecoveryEffects.Find(TargetAbilitySystem);
	if (EffectHandle != nullptr && EffectHandle->IsValid())
	{
		TargetAbilitySystem->RemoveActiveGameplayEffect(*EffectHandle);
	}

	ActiveRecoveryEffects.Remove(TargetAbilitySystem);
}

void ADRHotPackArea::RemoveAllRecoveries()
{
	for (const TPair<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle>& RecoveryEffect
		: ActiveRecoveryEffects)
	{
		UAbilitySystemComponent* TargetAbilitySystem = RecoveryEffect.Key.Get();
		if (IsValid(TargetAbilitySystem) && RecoveryEffect.Value.IsValid())
		{
			TargetAbilitySystem->RemoveActiveGameplayEffect(RecoveryEffect.Value);
		}
	}

	ActiveRecoveryEffects.Reset();
}

UAbilitySystemComponent* ADRHotPackArea::GetTargetAbilitySystem(AActor* TargetActor) const
{
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(TargetActor);
	return AbilitySystemInterface != nullptr
		? AbilitySystemInterface->GetAbilitySystemComponent()
		: nullptr;
}
