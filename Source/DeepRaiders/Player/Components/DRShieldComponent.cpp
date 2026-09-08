#include "DRShieldComponent.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"

UDRShieldComponent::UDRShieldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UDRShieldComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent())
	{
		AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
			this,
			&ThisClass::HandleActiveGameplayEffectAdded);
	}
}

bool UDRShieldComponent::GrantShield(
	const float Amount,
	const float Duration,
	const TSubclassOf<UGameplayEffect> DurationEffectClass)
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	if (!GetOwner()->HasAuthority()
		|| !IsValid(AbilitySystemComponent)
		|| !DurationEffectClass
		|| Amount <= KINDA_SMALL_NUMBER
		|| Duration <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle EffectSpec = AbilitySystemComponent->MakeOutgoingSpec(
		DurationEffectClass, 1.f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		return false;
	}

	EffectSpec.Data->SetDuration(Duration, true);
	const FActiveGameplayEffectHandle EffectHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	if (!EffectHandle.IsValid())
	{
		return false;
	}

	return AddShieldLayer(Amount, EffectHandle, this);
}

bool UDRShieldComponent::RegisterPersonalShieldGrant(
	const float Amount,
	const UObject* SourceObject)
{
	if (!GetOwner()->HasAuthority() || Amount <= KINDA_SMALL_NUMBER || !IsValid(SourceObject))
	{
		return false;
	}

	const FObjectKey SourceKey(SourceObject);
	const FActiveGameplayEffectHandle* DurationEffectHandle =
		LatestDurationHandlesBySource.Find(SourceKey);
	if (DurationEffectHandle == nullptr || !DurationEffectHandle->IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ShieldLayer] Grant ignored: no PersonalShield duration GE. Owner=%s Source=%s Amount=%.1f"),
			*GetNameSafe(GetOwner()), *GetNameSafe(SourceObject), Amount);
		return false;
	}

	return AddShieldLayer(Amount, *DurationEffectHandle, SourceObject);
}

float UDRShieldComponent::AbsorbDamage(float IncomingDamage)
{
	if (!GetOwner()->HasAuthority() || IncomingDamage <= KINDA_SMALL_NUMBER)
	{
		return IncomingDamage;
	}

	ShieldLayers.Sort([](const FDRShieldLayer& Left, const FDRShieldLayer& Right)
	{
		return Left.ExpireServerTime < Right.ExpireServerTime;
	});

	TArray<FActiveGameplayEffectHandle> DepletedLayers;
	for (FDRShieldLayer& Layer : ShieldLayers)
	{
		const float AbsorbedAmount = FMath::Min(Layer.RemainingAmount, IncomingDamage);
		UE_LOG(LogTemp, Log,
			TEXT("[ShieldLayer] Absorb Owner=%s Handle=%s Before=%.1f Damage=%.1f Absorb=%.1f"),
			*GetNameSafe(GetOwner()), *Layer.DurationEffectHandle.ToString(),
			Layer.RemainingAmount, IncomingDamage, AbsorbedAmount);
		Layer.RemainingAmount -= AbsorbedAmount;
		IncomingDamage -= AbsorbedAmount;

		if (Layer.RemainingAmount <= KINDA_SMALL_NUMBER)
		{
			DepletedLayers.Add(Layer.DurationEffectHandle);
		}

		if (IncomingDamage <= KINDA_SMALL_NUMBER)
		{
			break;
		}
	}

	for (const FActiveGameplayEffectHandle& EffectHandle : DepletedLayers)
	{
		RemoveShieldLayer(EffectHandle, true);
	}

	RefreshTotalShield();
	return FMath::Max(0.f, IncomingDamage);
}

void UDRShieldComponent::ClearShieldLayers()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	const TArray<FDRShieldLayer> LayersToRemove = ShieldLayers;
	ShieldLayers.Reset();
	RefreshTotalShield();

	if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent())
	{
		for (const FDRShieldLayer& Layer : LayersToRemove)
		{
			if (Layer.DurationEffectHandle.IsValid())
			{
				AbilitySystemComponent->RemoveActiveGameplayEffect(Layer.DurationEffectHandle);
			}
		}
	}
}

UAbilitySystemComponent* UDRShieldComponent::GetAbilitySystemComponent() const
{
	const ADRPlayerState* PlayerState = Cast<ADRPlayerState>(GetOwner());
	return IsValid(PlayerState) ? PlayerState->GetAbilitySystemComponent() : nullptr;
}

void UDRShieldComponent::HandleLayerEffectRemoved(const FActiveGameplayEffectHandle EffectHandle)
{
	for (auto It = LatestDurationHandlesBySource.CreateIterator(); It; ++It)
	{
		if (It.Value() == EffectHandle)
		{
			It.RemoveCurrent();
		}
	}

	RemoveShieldLayer(EffectHandle, false);
}

void UDRShieldComponent::HandleActiveGameplayEffectAdded(
	UAbilitySystemComponent* TargetAbilitySystem,
	const FGameplayEffectSpec& EffectSpec,
	const FActiveGameplayEffectHandle EffectHandle)
{
	if (!GetOwner()->HasAuthority()
		|| !IsValid(TargetAbilitySystem)
		|| EffectSpec.Def == nullptr
		|| !EffectSpec.Def->InheritableOwnedTagsContainer.CombinedTags.HasTagExact(
			DRGameplayTags::State_PersonalShield))
	{
		return;
	}

	const UObject* SourceObject = EffectSpec.GetContext().GetSourceObject();
	if (!IsValid(SourceObject))
	{
		return;
	}

	LatestDurationHandlesBySource.Add(FObjectKey(SourceObject), EffectHandle);
	UE_LOG(LogTemp, Log,
		TEXT("[ShieldLayer] Duration registered. Owner=%s Source=%s Handle=%s"),
		*GetNameSafe(GetOwner()), *GetNameSafe(SourceObject), *EffectHandle.ToString());
}

bool UDRShieldComponent::AddShieldLayer(
	const float Amount,
	const FActiveGameplayEffectHandle DurationEffectHandle,
	const UObject* SourceObject)
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	if (!IsValid(AbilitySystemComponent)
		|| !DurationEffectHandle.IsValid()
		|| Amount <= KINDA_SMALL_NUMBER
		|| !IsValid(SourceObject))
	{
		return false;
	}

	const FActiveGameplayEffect* ActiveEffect =
		AbilitySystemComponent->GetActiveGameplayEffect(DurationEffectHandle);
	if (ActiveEffect == nullptr)
	{
		return false;
	}

	const FObjectKey SourceKey(SourceObject);
	const int32 ExistingLayerIndex = ShieldLayers.IndexOfByPredicate(
		[SourceKey](const FDRShieldLayer& Layer)
		{
			return Layer.SourceKey == SourceKey;
		});
	if (ExistingLayerIndex != INDEX_NONE)
	{
		RemoveShieldLayer(ShieldLayers[ExistingLayerIndex].DurationEffectHandle, true);
	}

	const float WorldTime = GetWorld()->GetTimeSeconds();
	FDRShieldLayer& NewLayer = ShieldLayers.Emplace_GetRef();
	NewLayer.DurationEffectHandle = DurationEffectHandle;
	NewLayer.RemainingAmount = Amount;
	NewLayer.ExpireServerTime = WorldTime + ActiveEffect->GetTimeRemaining(WorldTime);
	NewLayer.SourceKey = SourceKey;

	if (FOnActiveGameplayEffectRemoved_Info* RemovedDelegate =
		AbilitySystemComponent->OnGameplayEffectRemoved_InfoDelegate(DurationEffectHandle))
	{
		RemovedDelegate->AddWeakLambda(
			this,
			[this, DurationEffectHandle](const auto& /*RemovalInfo*/)
			{
				HandleLayerEffectRemoved(DurationEffectHandle);
			});
	}

	UE_LOG(LogTemp, Log,
		TEXT("[ShieldLayer] Layer added. Owner=%s Source=%s Handle=%s Amount=%.1f Remaining=%.2fs"),
		*GetNameSafe(GetOwner()), *GetNameSafe(SourceObject), *DurationEffectHandle.ToString(),
		Amount, ActiveEffect->GetTimeRemaining(WorldTime));
	RefreshTotalShield();
	return true;
}

void UDRShieldComponent::RemoveShieldLayer(
	const FActiveGameplayEffectHandle EffectHandle,
	const bool bRemoveDurationEffect)
{
	if (!EffectHandle.IsValid() || bIsRemovingLayer)
	{
		return;
	}

	const int32 LayerIndex = ShieldLayers.IndexOfByPredicate(
		[EffectHandle](const FDRShieldLayer& Layer)
		{
			return Layer.DurationEffectHandle == EffectHandle;
		});
	if (LayerIndex == INDEX_NONE)
	{
		return;
	}

	bIsRemovingLayer = true;
	UE_LOG(LogTemp, Log,
		TEXT("[ShieldLayer] Layer removed. Owner=%s Handle=%s Remaining=%.1f RemoveGE=%d"),
		*GetNameSafe(GetOwner()), *EffectHandle.ToString(),
		ShieldLayers[LayerIndex].RemainingAmount, bRemoveDurationEffect);
	ShieldLayers.RemoveAtSwap(LayerIndex);
	RefreshTotalShield();

	if (bRemoveDurationEffect)
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
		}
	}
	bIsRemovingLayer = false;
}

void UDRShieldComponent::RefreshTotalShield()
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	if (!GetOwner()->HasAuthority() || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	float TotalShield = 0.f;
	for (const FDRShieldLayer& Layer : ShieldLayers)
	{
		TotalShield += FMath::Max(0.f, Layer.RemainingAmount);
	}

	AbilitySystemComponent->SetNumericAttributeBase(
		UDRPlayerAttributeSet::GetShieldAttribute(), TotalShield);
	UE_LOG(LogTemp, Log,
		TEXT("[ShieldLayer] Total refreshed. Owner=%s Layers=%d Total=%.1f"),
		*GetNameSafe(GetOwner()), ShieldLayers.Num(), TotalShield);
}
