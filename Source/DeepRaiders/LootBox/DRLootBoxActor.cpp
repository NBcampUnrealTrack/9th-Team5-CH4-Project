
#include "DRLootBoxActor.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/SceneComponent.h"
#include "DeepRaiders/LootBox/Component/DRLootDropComponent.h"
#include "Net/UnrealNetwork.h"

ADRLootBoxActor::ADRLootBoxActor()
{
	LootDropComponent = CreateDefaultSubobject<UDRLootDropComponent>(TEXT("LootDropComponent"));
	LootDropComponent->SetSpawnMode(EDRLootSpawnMode::Sequential);
	LootSpawnPointComponent = CreateDefaultSubobject<USceneComponent>(TEXT("LootSpawnPointComponent"));
	
	LootSpawnPointComponent->SetupAttachment(GetRootComponent());
	
	IdleAuraVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("IdleAuraVFXComponent"));

	IdleAuraVFXComponent->SetupAttachment(BreakableMeshComponent);
	IdleAuraVFXComponent->SetAutoActivate(false);
	IdleAuraVFXComponent->SetIsReplicated(false);
}

void ADRLootBoxActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && IsValid(LootDropComponent))
	{
		LootSpawnSequenceCompletedHandle = LootDropComponent->OnSpawnSequenceCompleted.AddUObject(
			this, &ThisClass::HandleLootSpawnSequenceCompleted);
	}
	
	RefreshPresentation();
	
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnLootTierChanged(LootTier);
	}
}

void ADRLootBoxActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(LootDropComponent) && LootSpawnSequenceCompletedHandle.IsValid())
	{
		LootDropComponent->OnSpawnSequenceCompleted.Remove(LootSpawnSequenceCompletedHandle);
		LootSpawnSequenceCompletedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void ADRLootBoxActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, LootTier);
}

bool ADRLootBoxActor::SetLootTier(EDRLootTier NewLootTier)
{
	if (!HasAuthority()
		|| IsBroken())
	{
		return false;
	}
	
	if (LootTier == NewLootTier)
	{
		return true;
	}
	
	LootTier = NewLootTier;
	ForceNetUpdate();
	
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnLootTierChanged(LootTier);
	}
	
	return true;
}

void ADRLootBoxActor::HandleBroken(const FDRBreakableDamageContext& DamageContext)
{
	Super::HandleBroken(DamageContext);
	
	if (!HasAuthority()
		|| !IsValid(LootDropComponent)
		|| !IsValid(LootSpawnPointComponent))
	{
		return;
	}
	
	LootDropComponent->GenerateAndSpawnLoot(LootTier, LootSpawnPointComponent->GetComponentTransform());
	bWaitingForLootSpawnSequence = LootDropComponent->IsSpawnSequenceActive();
}

bool ADRLootBoxActor::ShouldDeferBrokenDestruction() const
{
	return bWaitingForLootSpawnSequence;
}

void ADRLootBoxActor::HandleLootSpawnSequenceCompleted()
{
	if (!HasAuthority() || !bWaitingForLootSpawnSequence)
	{
		return;
	}

	bWaitingForLootSpawnSequence = false;
	StartBrokenDestructionCountdown();
}

void ADRLootBoxActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	RefreshPresentation();
}

void ADRLootBoxActor::OnRep_LootTier()
{
	RefreshPresentation();
	
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnLootTierChanged(LootTier);
	}
}

void ADRLootBoxActor::RefreshPresentation()
{
	RefreshDynamicMaterialColor();
	RefreshNiagara();
}

void ADRLootBoxActor::RefreshNiagara()
{
	if (IsBroken())
	{
		IdleAuraVFXComponent->Deactivate();
		return;
	}
	
	if (IsValid(IdleAuraVFXComponent->GetAsset())
		&& LootTier != EDRLootTier::Common)
	{
		IdleAuraVFXComponent->Activate(true);
		
		IdleAuraVFXComponent->SetVariableLinearColor(TEXT("User.RarityColor"),GetRarityColor(LootTier));
	}
	else
	{
		IdleAuraVFXComponent->Deactivate();
	}
}

void ADRLootBoxActor::RefreshDynamicMaterialColor()
{	
	if (IsBroken())
	{
		return;
	}	
	
	if (!IsValid(DynamicMaterial))
	{
		DynamicMaterial = BreakableMeshComponent->CreateDynamicMaterialInstance(0);
	}
	
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(BaseColorParameterName, GetRarityBaseColor(LootTier));
		DynamicMaterial->SetVectorParameterValue(EmissiveColorParameterName, GetRarityEmissiveColor(LootTier));
	}
}

FLinearColor ADRLootBoxActor::GetRarityColor(EDRLootTier Rarity)
{
	switch (Rarity)
	{
	case EDRLootTier::Common:
		return FLinearColor::White;

	case EDRLootTier::Uncommon:
		return FLinearColor::Green;
		
	case EDRLootTier::Rare:
		return FLinearColor::Blue;

	case EDRLootTier::Epic:
		return FLinearColor(0.5f, 0.f, 1.f);

	case EDRLootTier::Legendary:
		return FLinearColor(1.f, 0.4f, 0.f);

	default:
		return FLinearColor::White;
	}
}

FLinearColor ADRLootBoxActor::GetRarityBaseColor(EDRLootTier Rarity)
{
	return GetRarityColor(Rarity);
}

FLinearColor ADRLootBoxActor::GetRarityEmissiveColor(EDRLootTier Rarity)
{
	switch (Rarity)
	{
	case EDRLootTier::Common:
		return FLinearColor(0.f, 0.f, 0.f);

	case EDRLootTier::Uncommon:
		return FLinearColor(0.f, 0.f, 0.f);
		
	case EDRLootTier::Rare:
	case EDRLootTier::Epic:
		return GetRarityColor(Rarity) * EmissiveIntensity;
	case EDRLootTier::Legendary:
		return GetRarityColor(Rarity) * EmissiveIntensity * 2;
	default:
		return FLinearColor::White;
	}
}
