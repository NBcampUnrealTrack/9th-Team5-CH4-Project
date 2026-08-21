#include "DRPlayerState.h"

#include "DRPlayerCharacter.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"

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

	PerkComponent =
		CreateDefaultSubobject<UDRPerkComponent>(
			TEXT("PerkComponent"));
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
	DOREPLIFETIME_CONDITION(ADRPlayerState, Coins, COND_OwnerOnly);
	DOREPLIFETIME(ADRPlayerState, TeamId);
	DOREPLIFETIME(ADRPlayerState, PublicQuickSlots);
}

void ADRPlayerState::UpdatePublicQuickSlots(const UDRQuickSlotComponent* QuickSlotComponent)
{
	if (!HasAuthority() || !IsValid(QuickSlotComponent))
	{
		return;
	}

	PublicQuickSlots.SetNum(QuickSlotComponent->GetSlotCount());
	for (int32 SlotIndex = 0; SlotIndex < PublicQuickSlots.Num(); ++SlotIndex)
	{
		FDRItemInstance ItemInstance;
		const bool bHasItem = QuickSlotComponent->GetQuickSlot(SlotIndex, ItemInstance);
		FDRPublicQuickSlot& SnapshotSlot = PublicQuickSlots[SlotIndex];
		SnapshotSlot.ItemDefinition = bHasItem ? ItemInstance.Definition.Get() : nullptr;
		SnapshotSlot.Quantity = bHasItem ? ItemInstance.Quantity : 0;
	}

	OnPublicQuickSlotsChanged.Broadcast();
	ForceNetUpdate();
}

void ADRPlayerState::OnRep_PublicQuickSlots()
{
	OnPublicQuickSlotsChanged.Broadcast();
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

void ADRPlayerState::AddCoins(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}

	const int64 NewCoins = static_cast<int64>(Coins) + Amount;
	SetCoins(static_cast<int32>(FMath::Min<int64>(NewCoins, MAX_int32)));
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
	
	// 이전 생명주기의 Dead 상태 Effect 제거
	{
		FGameplayTagContainer TempTags;
		TempTags.AddTag(DRGameplayTags::State_Dead);

		AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(TempTags);
	}

	// Respawn Attribute 초기화
	AbilitySystemComponent->SetNumericAttributeBase(UDRPlayerAttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth());

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

	StopFreezeDecay();
	
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

		EvaluateDeadState();
		EvaluateFrozenState();
	}
}

void ADRPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFreezeDecay();
	UnbindStatusPolicy();

	Super::EndPlay(EndPlayReason);
}

void ADRPlayerState::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!HasAuthority())
	{
		return;
	}

	/*
	 * 살아있다가 Health가 0 이하가 된
	 * 순간만 Death 상태 평가.
	 */
	if (Data.OldValue > KINDA_SMALL_NUMBER && Data.NewValue <= KINDA_SMALL_NUMBER)
	{
		EvaluateDeadState();
	}
}

void ADRPlayerState::EvaluateDeadState()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || !DeadEffectClass)
	{
		return;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Dead))
	{
		return;
	}

	const UDRPlayerAttributeSet* Attributes = AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>();

	if (!IsValid(Attributes) || Attributes->GetHealth() > KINDA_SMALL_NUMBER)
	{
		return;
	}

	/*
	 * 죽은 동안 Freeze Decay 같은
	 * 살아있는 플레이어용 Timer는 중단.
	 */
	StopFreezeDecay();

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DeadEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void ADRPlayerState::GrantDefaultAbilities()
{
	if (!HasAuthority() ||
		!IsValid(AbilitySystemComponent))
	{
		return;
	}
	
	if (DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedHandles, this);
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
	
	HealthChangedHandle =
		AbilitySystemComponent->
			GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::
					GetHealthAttribute())
			.AddUObject(
				this,
				&ThisClass::HandleHealthChanged);
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
		FreezeGaugeChangedHandle.Reset();
	}

	if (MaxFreezeGaugeChangedHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::
					GetMaxFreezeGaugeAttribute())
			.Remove(MaxFreezeGaugeChangedHandle);
		MaxFreezeGaugeChangedHandle.Reset();
	}
	
	if (HealthChangedHandle.IsValid())
	{
		AbilitySystemComponent->
			GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::
					GetHealthAttribute())
			.Remove(
				HealthChangedHandle);
		HealthChangedHandle.Reset();
	}
}

void ADRPlayerState::HandleFreezeGaugeChanged(const FOnAttributeChangeData& Data)
{
	if (!HasAuthority())
	{
		return;
	}

	EvaluateFrozenState();

	// 100에 도달해서 Frozen이 됐다면
	// 자연 감소는 더 이상 하지 않는다.
	if (IsFrozen())
	{
		StopFreezeDecay();
		return;
	}

	// 값이 증가했다 = 새로운 빙결 공격을 맞았다.
	// 마지막 피격 시점부터 Delay를 다시 센다.
	if (Data.NewValue > Data.OldValue + KINDA_SMALL_NUMBER)
	{
		RestartFreezeDecay();
		return;
	}

	// 자연 감소 등으로 0에 도달했다면 종료.
	if (Data.NewValue <= KINDA_SMALL_NUMBER)
	{
		StopFreezeDecay();
	}
}

void ADRPlayerState::HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData&)
{
	EvaluateFrozenState();
}

void ADRPlayerState::EvaluateFrozenState()
{
	if (AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Dead))
	{
		return;
	}
	
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

void ADRPlayerState::RestartFreezeDecay()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || IsFrozen())
	{
		return;
	}

	const float CurrentFreeze = AbilitySystemComponent->GetNumericAttribute(UDRPlayerAttributeSet::GetFreezeGaugeAttribute());

	if (CurrentFreeze <= KINDA_SMALL_NUMBER)
	{
		StopFreezeDecay();
		return;
	}

	if (FreezeDecayInterval <= 0.f || FreezeDecayRatePerSecond <= 0.f)
	{
		StopFreezeDecay();
		return;
	}

	GetWorldTimerManager().SetTimer(
		FreezeDecayTimerHandle, this, &ThisClass::TickFreezeDecay, FreezeDecayInterval, true, FreezeDecayDelay);
}

void ADRPlayerState::TickFreezeDecay()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || IsFrozen())
	{
		StopFreezeDecay();
		return;
	}

	const FGameplayAttribute FreezeAttribute = UDRPlayerAttributeSet::GetFreezeGaugeAttribute();
	const float CurrentFreeze = AbilitySystemComponent->GetNumericAttribute(FreezeAttribute);
	if (CurrentFreeze <= KINDA_SMALL_NUMBER)
	{
		StopFreezeDecay();
		return;
	}

	const float DecayAmount = FreezeDecayRatePerSecond * FreezeDecayInterval;
	const float NewFreeze = FMath::Max(0.f, CurrentFreeze - DecayAmount);
	AbilitySystemComponent->SetNumericAttributeBase(FreezeAttribute, NewFreeze);
}

void ADRPlayerState::StopFreezeDecay()
{
	GetWorldTimerManager().ClearTimer(FreezeDecayTimerHandle);
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
