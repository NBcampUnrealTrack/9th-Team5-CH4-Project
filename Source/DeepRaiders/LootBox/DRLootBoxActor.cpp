
#include "DRLootBoxActor.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/SceneComponent.h"
#include "DeepRaiders/Core/GameStates/DRMiningGameStateBase.h"
#include "DeepRaiders/LootBox/Component/DRLootDropComponent.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelWorld.h"

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
	BindGameFlowState();
	
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnLootTierChanged(LootTier);
		BindLocalPlayerVisibilityTags();
	}
}

void ADRLootBoxActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameFlowState();
	UnbindLocalPlayerVisibilityTags();
	GetWorldTimerManager().ClearTimer(LocalPlayerVisibilityBindRetryTimer);

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
	DOREPLIFETIME(ThisClass, bIsVoxelExposed);
}

float ADRLootBoxActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!bIsGameFlowAvailable)
	{
		return 0.f;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ADRLootBoxActor::UpdateVoxelExposure(AVoxelWorld& VoxelWorld)
{
	if (!HasAuthority() || !VoxelWorld.IsCreated())
	{
		return;
	}

	const FVoxelIntBox WorldBounds = VoxelWorld.GetWorldBounds();
	bool bNewVoxelExposure = false;
	const FBox MeshBounds = BreakableMeshComponent->Bounds.GetBox();
	TArray<FIntVector, TInlineAllocator<125>> SamplePositions;
	FVoxelIntBoxWithValidity LockBounds;
	for (int32 X = 0; X < 5; ++X)
	{
		for (int32 Y = 0; Y < 5; ++Y)
		{
			for (int32 Z = 0; Z < 5; ++Z)
			{
				if (X != 0 && X != 4 && Y != 0 && Y != 4 && Z != 0 && Z != 4)
				{
					continue;
				}
				const FVector Point(FMath::Lerp(MeshBounds.Min.X, MeshBounds.Max.X, X / 4.f),
					FMath::Lerp(MeshBounds.Min.Y, MeshBounds.Max.Y, Y / 4.f),
					FMath::Lerp(MeshBounds.Min.Z, MeshBounds.Max.Z, Z / 4.f));
				const FIntVector VoxelPosition = VoxelWorld.GlobalToLocal(Point);
				if (!WorldBounds.Contains(VoxelPosition))
				{
					bNewVoxelExposure = true;
					continue;
				}
				SamplePositions.Add(VoxelPosition);
				LockBounds += VoxelPosition;
			}
		}
	}
	if (!bNewVoxelExposure && LockBounds.IsValid())
	{
		FVoxelData& Data = VoxelWorld.GetData();
		FVoxelReadScopeLock Lock(Data, LockBounds.GetBox(), FUNCTION_FNAME);
		bNewVoxelExposure = SamplePositions.ContainsByPredicate([&Data](const FIntVector& Position)
		{
			return Data.GetValue(Position, 0).IsEmpty();
		});
	}
	if (bIsVoxelExposed == bNewVoxelExposure)
	{
		return;
	}

	bIsVoxelExposed = bNewVoxelExposure;
	ApplyVisibility();
	ForceNetUpdate();
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

void ADRLootBoxActor::BindGameFlowState()
{
	UnbindGameFlowState();

	UWorld* World = GetWorld();
	MiningGameState = IsValid(World) ? World->GetGameState<ADRMiningGameStateBase>() : nullptr;
	if (IsValid(MiningGameState))
	{
		MiningGameState->OnGameFlowStateChanged.AddUniqueDynamic(
			this, &ThisClass::HandleGameFlowStateChanged);
		HandleGameFlowStateChanged(MiningGameState->GetGameFlowState());
		return;
	}

	HandleGameFlowStateChanged(EDRGameFlowState::WaitingForPlayers);
}

void ADRLootBoxActor::UnbindGameFlowState()
{
	if (IsValid(MiningGameState))
	{
		MiningGameState->OnGameFlowStateChanged.RemoveDynamic(
			this, &ThisClass::HandleGameFlowStateChanged);
	}

	MiningGameState = nullptr;
}

void ADRLootBoxActor::HandleGameFlowStateChanged(EDRGameFlowState GameFlowState)
{
	bIsGameFlowAvailable = GameFlowState == EDRGameFlowState::Countdown
		|| GameFlowState == EDRGameFlowState::Playing;
	ApplyGameFlowAvailability();
}

void ADRLootBoxActor::ApplyGameFlowAvailability()
{
	SetActorEnableCollision(bIsGameFlowAvailable && !IsBroken());
	ApplyVisibility();
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

void ADRLootBoxActor::BindLocalPlayerVisibilityTags()
{
	UWorld* World = GetWorld();
	APlayerController* LocalController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	ADRPlayerCharacter* LocalCharacter = IsValid(LocalController)
		? Cast<ADRPlayerCharacter>(LocalController->GetPawn())
		: nullptr;
	UAbilitySystemComponent* AbilitySystem = IsValid(LocalCharacter)
		? LocalCharacter->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(LocalController) || !LocalController->IsLocalController() || !IsValid(AbilitySystem))
	{
		GetWorldTimerManager().SetTimer(
			LocalPlayerVisibilityBindRetryTimer,
			this,
			&ThisClass::BindLocalPlayerVisibilityTags,
			0.25f,
			false);
		return;
	}

	if (LocalPlayerAbilitySystem.Get() != AbilitySystem)
	{
		UnbindLocalPlayerVisibilityTags();
		LocalPlayerAbilitySystem = AbilitySystem;
		LocalPlayerDeadTagChangedHandle = AbilitySystem->RegisterGameplayTagEvent(
			DRGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleLocalPlayerVisibilityTagChanged);
		LocalPlayerVoxelContainedTagChangedHandle = AbilitySystem->RegisterGameplayTagEvent(
			DRGameplayTags::State_VoxelContained, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleLocalPlayerVisibilityTagChanged);
	}

	RefreshLocalPlayerVisibility();
}

void ADRLootBoxActor::UnbindLocalPlayerVisibilityTags()
{
	if (UAbilitySystemComponent* AbilitySystem = LocalPlayerAbilitySystem.Get())
	{
		if (LocalPlayerDeadTagChangedHandle.IsValid())
		{
			AbilitySystem->RegisterGameplayTagEvent(DRGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
				.Remove(LocalPlayerDeadTagChangedHandle);
		}
		if (LocalPlayerVoxelContainedTagChangedHandle.IsValid())
		{
			AbilitySystem->RegisterGameplayTagEvent(DRGameplayTags::State_VoxelContained, EGameplayTagEventType::NewOrRemoved)
				.Remove(LocalPlayerVoxelContainedTagChangedHandle);
		}
	}

	LocalPlayerDeadTagChangedHandle.Reset();
	LocalPlayerVoxelContainedTagChangedHandle.Reset();
	LocalPlayerAbilitySystem.Reset();
}

void ADRLootBoxActor::RefreshLocalPlayerVisibility()
{
	const UAbilitySystemComponent* AbilitySystem = LocalPlayerAbilitySystem.Get();
	bHideForContainedDeath = IsValid(AbilitySystem)
		&& AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_VoxelContained)
		&& AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Dead);
	ApplyVisibility();
}

void ADRLootBoxActor::HandleLocalPlayerVisibilityTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	RefreshLocalPlayerVisibility();
}

void ADRLootBoxActor::OnRep_VoxelExposed()
{
	ApplyVisibility();
}

void ADRLootBoxActor::ApplyVisibility()
{
	SetActorHiddenInGame(!bIsGameFlowAvailable || !bIsVoxelExposed || bHideForContainedDeath);
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
