#include "DRPlayerState.h"

#include "DRPlayerCharacter.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

ADRPlayerState::ADRPlayerState()
{
	AbilitySystemComponent =
		CreateDefaultSubobject<UAbilitySystemComponent>(
			TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(true);

	AbilitySystemComponent->SetReplicationMode(
		EGameplayEffectReplicationMode::Mixed);
	
	PlayerAttributeSet =
		CreateDefaultSubobject<UDRPlayerAttributeSet>(
			TEXT("PlayerAttributeSet"));
}

UAbilitySystemComponent* ADRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADRPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRPlayerState, bHasDeepestDigLocation);
	DOREPLIFETIME(ADRPlayerState, DeepestDigLocation);
	DOREPLIFETIME(ADRPlayerState, bHasJetpack);
	DOREPLIFETIME_CONDITION(ADRPlayerState, CurrentJetpackFuel, COND_OwnerOnly);
	DOREPLIFETIME(ADRPlayerState, Coins);
	DOREPLIFETIME(ADRPlayerState, TeamId);
}

bool ADRPlayerState::UpdateDeepestDigLocation(const FVector& Location)
{
	if (!HasAuthority()
		|| (bHasDeepestDigLocation && Location.Z >= DeepestDigLocation.Z))
	{
		return false;
	}

	bHasDeepestDigLocation = true;
	DeepestDigLocation = Location;
	ForceNetUpdate();
	return true;
}

void ADRPlayerState::GrantJetpack()
{
	if (!HasAuthority())
	{
		return;
	}

	bHasJetpack = true;
	CurrentJetpackFuel = MaxJetpackFuel;
	RefreshJetpackVisualOnPawn();
	ForceNetUpdate();
}

bool ADRPlayerState::ConsumeJetpackFuel(float Amount)
{
	if (!HasAuthority()
		|| !bHasJetpack
		|| Amount <= 0.f
		|| CurrentJetpackFuel <= 0.f)
	{
		return false;
	}

	CurrentJetpackFuel = FMath::Max(0.f, CurrentJetpackFuel - Amount);
	return true;
}

bool ADRPlayerState::RefillJetpackFuel()
{
	if (!HasAuthority()
		|| !bHasJetpack
		|| FMath::IsNearlyEqual(CurrentJetpackFuel, MaxJetpackFuel))
	{
		return false;
	}

	CurrentJetpackFuel = MaxJetpackFuel;
	ForceNetUpdate();
	return true;
}

int32 ADRPlayerState::GetCoins() const
{
	return Coins;
}

void ADRPlayerState::SetCoins(int32 NewCoins)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 PreviousCoins = Coins;
	const int32 ClampedCoins = FMath::Max(0, NewCoins);

	if (PreviousCoins == ClampedCoins)
	{
		return;
	}

	Coins = ClampedCoins;
	OnRep_Coins(PreviousCoins);
	ForceNetUpdate();
}

void ADRPlayerState::ResetForRespawn()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	const UDRPlayerAttributeSet* Attributes = AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>();

	if (!IsValid(Attributes))
	{
		return;
	}

	ClearFrozenState();
	
	// Dead 상태도 이후 BP_GE_Dead를 사용하게 될 것을 고려해 제거
	{
		FGameplayTagContainer TempTags;
		TempTags.AddTag(DRGameplayTags::State_Dead);

		AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(TempTags);
	}

	// Respawn Attribute 초기화
	AbilitySystemComponent->SetNumericAttributeBase(UDRPlayerAttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth());
	AbilitySystemComponent->SetNumericAttributeBase(UDRPlayerAttributeSet::GetFreezeGaugeAttribute(), 0.f);

	// 현재 Snow Absorb가 없으므로 전투 루프를 위해 Full로 리스폰.
	// Snow Absorb 구현 후 정책에 맞게 0.f 등으로 변경.
	AbilitySystemComponent->SetNumericAttributeBase(UDRPlayerAttributeSet::GetSnowGaugeAttribute(), Attributes->GetMaxSnowGauge());
}

bool ADRPlayerState::IsFrozen() const
{
	return IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Frozen);
}

void ADRPlayerState::ClearFrozenState()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	FGameplayTagContainer FrozenTags;
	FrozenTags.AddTag(DRGameplayTags::State_Frozen);

	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(FrozenTags);

	AbilitySystemComponent->SetNumericAttributeBase(UDRPlayerAttributeSet::GetFreezeGaugeAttribute(), 0.f);
}

void ADRPlayerState::BeginPlay()
{
	Super::BeginPlay();

	GrantDefaultAbilities();

	if (HasAuthority())
	{
		BindStatusPolicy();
		EvaluateFrozenState();
	}
}

void ADRPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindStatusPolicy();

	Super::EndPlay(EndPlayReason);
}

void ADRPlayerState::GrantDefaultAbilities()
{
	if (!HasAuthority() ||
		!IsValid(AbilitySystemComponent))
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass
		 : DefaultAbilities)
	{
		if (!IsValid(AbilityClass))
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(
			AbilityClass,
			1);

		const FGameplayAbilitySpecHandle Handle =
			AbilitySystemComponent->GiveAbility(
				AbilitySpec);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[GAS][GiveAbility] "
				"PlayerState=%s "
				"Ability=%s "
				"HandleValid=%d"),
			*GetNameSafe(this),
			*GetNameSafe(AbilityClass),
			Handle.IsValid());
	}
}

void ADRPlayerState::BindStatusPolicy()
{
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	FreezeGaugeChangedHandle =
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::
					GetFreezeGaugeAttribute())
			.AddUObject(
				this,
				&ThisClass::HandleFreezeGaugeChanged);

	MaxFreezeGaugeChangedHandle =
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::
					GetMaxFreezeGaugeAttribute())
			.AddUObject(
				this,
				&ThisClass::HandleMaxFreezeGaugeChanged);
}

void ADRPlayerState::UnbindStatusPolicy()
{
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	if (FreezeGaugeChangedHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::
					GetFreezeGaugeAttribute())
			.Remove(FreezeGaugeChangedHandle);
	}

	if (MaxFreezeGaugeChangedHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::
					GetMaxFreezeGaugeAttribute())
			.Remove(MaxFreezeGaugeChangedHandle);
	}

	FreezeGaugeChangedHandle.Reset();
	MaxFreezeGaugeChangedHandle.Reset();
}

void ADRPlayerState::HandleFreezeGaugeChanged(const FOnAttributeChangeData&)
{
	EvaluateFrozenState();
}

void ADRPlayerState::HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData&)
{
	EvaluateFrozenState();
}

void ADRPlayerState::EvaluateFrozenState()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || !FrozenEffectClass)
	{
		return;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Frozen))
	{
		return;
	}

	const UDRPlayerAttributeSet* Attributes = AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>();
	if (!IsValid(Attributes))
	{
		return;
	}
	
	if (Attributes->GetFreezeGauge() < Attributes->GetMaxFreezeGauge())
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(FrozenEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void ADRPlayerState::OnRep_Coins(int32 PreviousCoins)
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Coins changed: Previous=%d New=%d"),
		PreviousCoins,
		Coins);

	OnCoinsChanged.Broadcast(Coins);
}

void ADRPlayerState::OnRep_HasJetpack()
{
	RefreshJetpackVisualOnPawn();
}

void ADRPlayerState::OnRep_JetpackFuel()
{
	ADRPlayerCharacter* PlayerCharacter =
		GetPawn<ADRPlayerCharacter>();

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->ReconcileJetpackFuelFromServer(
		CurrentJetpackFuel);
}

void ADRPlayerState::RefreshJetpackVisualOnPawn()
{
	ADRPlayerCharacter* PlayerCharacter = GetPawn<ADRPlayerCharacter>();

	if (IsValid(PlayerCharacter))
	{
		PlayerCharacter->RefreshJetpackVisual();
	}
}

#pragma region Teleport
void ADRPlayerState::SetTeamId(int32 NewTeamId)
{
	if (!HasAuthority() || TeamId == NewTeamId)
	{
		return;
	}

	TeamId = NewTeamId;
	ForceNetUpdate();
}
#pragma endregion
