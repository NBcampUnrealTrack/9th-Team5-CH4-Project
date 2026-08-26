
#include "DRLootBoxActor.h"

#include "Components/SceneComponent.h"
#include "DeepRaiders/LootBox/Component/DRLootDropComponent.h"
#include "Net/UnrealNetwork.h"

ADRLootBoxActor::ADRLootBoxActor()
{
	LootDropComponent = CreateDefaultSubobject<UDRLootDropComponent>(TEXT("LootDropComponent"));
	LootSpawnPointComponent = CreateDefaultSubobject<USceneComponent>(TEXT("LootSpawnPointComponent"));
	
	LootSpawnPointComponent->SetupAttachment(GetRootComponent());
}

void ADRLootBoxActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnLootTierChanged(LootTier);
	}
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
}

void ADRLootBoxActor::OnRep_LootTier()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnLootTierChanged(LootTier);
	}
}
