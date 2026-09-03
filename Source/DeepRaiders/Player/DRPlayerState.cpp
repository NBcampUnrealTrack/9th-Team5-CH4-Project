#include "DRPlayerState.h"

#include "DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRSilhouetteComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Skill/Components/DRSkillComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/Components//DRCombatStatsComponent.h"
#include "DeepRaiders/Input/DRInputTypes.h"
#include "DRPlayerController.h"

namespace DRPlayerName
{
	static const TCHAR* AnimalNames[] =
	{
		TEXT("Penguin"),
		TEXT("Fox"),
		TEXT("Otter"),
		TEXT("Bear"),
		TEXT("Rabbit"),
		TEXT("Wolf"),
		TEXT("Seal"),
		TEXT("Raccoon"),
		TEXT("Panda"),
		TEXT("Hamster")
	};
}

ADRPlayerState::ADRPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AbilitySystemComponent->GenericConfirmInputID = static_cast<int32>(EDRAbilityInputId::Primary);
	AbilitySystemComponent->GenericCancelInputID = static_cast<int32>(EDRAbilityInputId::Secondary);

	PlayerAttributeSet = CreateDefaultSubobject<UDRPlayerAttributeSet>(TEXT("PlayerAttributeSet"));
	PerkComponent = CreateDefaultSubobject<UDRPerkComponent>(TEXT("PerkComponent"));
	SkillComponent = CreateDefaultSubobject<UDRSkillComponent>(TEXT("SkillComponent"));
	CombatStatsComponent = CreateDefaultSubobject<UDRCombatStatsComponent>(TEXT("CombatStatsComponent"));
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

	// 실제 코인 값은 서버와 해당 PlayerState의 소유 클라이언트만 공유한다.
	DOREPLIFETIME_CONDITION(ADRPlayerState, Coins, COND_OwnerOnly);
	DOREPLIFETIME(ADRPlayerState, TeamId);
	DOREPLIFETIME(ADRPlayerState, PublicQuickSlots);
}

void ADRPlayerState::HandleDamageResolved(ADRPlayerState* SourcePlayerState, float AppliedDamage, bool bFatal)
{
	if (!HasAuthority() || AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	/*
	 * CombatStats 유효 여부와 무관하게
	 * 적중에 따른 이름 노출은 처리한다.
	 */
	HandleHostileHitResolved(SourcePlayerState);

	if (!IsValid(CombatStatsComponent))
	{
		return;
	}

	// 피해자는 DamageTaken을 항상 기록.
	CombatStatsComponent->RecordDamageTaken(AppliedDamage, bFatal);

	// 환경 피해, 자해, 낙하 피해는 공격자 통계 없음.
	if (!IsValid(SourcePlayerState) || SourcePlayerState == this)
	{
		return;
	}

	UDRCombatStatsComponent* SourceStats = SourcePlayerState->GetCombatStatsComponent();

	if (!IsValid(SourceStats))
	{
		return;
	}

	SourceStats->RecordDamageDealt(AppliedDamage, bFatal);

	UE_LOG(LogTemp, Log, TEXT( "[CombatStats] Source=%s Target=%s " "AppliedDamage=%.1f Fatal=%d"), 
		*GetNameSafe(SourcePlayerState), *GetNameSafe(this), AppliedDamage, bFatal);
}

void ADRPlayerState::HandleHostileHitResolved(
	ADRPlayerState* SourcePlayerState)
{
	if (!HasAuthority()
		|| !IsValid(SourcePlayerState)
		|| SourcePlayerState == this)
	{
		return;
	}

	/*
	 * 같은 팀의 Friendly Fire가 실제로 발생하더라도
	 * 적 이름 Reveal 대상으로 취급하지 않는다.
	 */
	if (SourcePlayerState->GetTeamId() == GetTeamId())
	{
		return;
	}

	ADRPlayerController* SourceController =
		Cast<ADRPlayerController>(
			SourcePlayerState->GetOwner());

	if (!IsValid(SourceController))
	{
		if (APawn* SourcePawn =
			SourcePlayerState->GetPawn())
		{
			SourceController =
				Cast<ADRPlayerController>(
					SourcePawn->GetController());
		}
	}

	if (!IsValid(SourceController))
	{
		return;
	}

	SourceController->RevealEnemyNameFromServer(this);
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

	// int32 덧셈 전에 int64로 확장해 오버플로를 방지한다.
	const int64 NewCoins = static_cast<int64>(Coins) + Amount;
	SetCoins(static_cast<int32>(FMath::Min<int64>(NewCoins, MAX_int32)));
}

void ADRPlayerState::ResetForGameStart()
{
	if (!HasAuthority())
	{
		return;
	}

	SetCoins(GetClass()->GetDefaultObject<ADRPlayerState>()->GetCoins());
	if (IsValid(PerkComponent))
	{
		PerkComponent->ResetPerks();
	}
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
	
	FGameplayTagContainer PersistThroughDeathTags;
	PersistThroughDeathTags.AddTag(DRGameplayTags::Effect_Policy_PersistThroughDeath);
	
	const FGameplayEffectQuery RemoveOnRespawnQuery = FGameplayEffectQuery::MakeQuery_MatchNoEffectTags(PersistThroughDeathTags);
	AbilitySystemComponent->RemoveActiveEffects(RemoveOnRespawnQuery);

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
		EvaluateFrozenState(PlayerAttributeSet->GetFreezeGauge(), PlayerAttributeSet->GetHealth());
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

	if (Data.NewValue <= KINDA_SMALL_NUMBER)
	{
		if (Data.OldValue > KINDA_SMALL_NUMBER)
		{
			EvaluateDeadState();
		}

		return;
	}

	EvaluateFrozenState(PlayerAttributeSet->GetFreezeGauge(), Data.NewValue);
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

	const bool bWasFrozen = IsFrozen();
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	
	if (bWasFrozen)
	{
		FGameplayCueParameters CueParameters;

		if (APawn* Pawn = GetPawn())
		{
			CueParameters.Location = Pawn->GetActorLocation();
			CueParameters.Instigator = Pawn;
			CueParameters.EffectCauser = Pawn;
		}

		AbilitySystemComponent->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Sound_Player_Frozen_Death, CueParameters);
	}
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

	EvaluateFrozenState(Data.NewValue, PlayerAttributeSet->GetHealth());

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
	// 비주얼용 MaxFreezeGauge 변경.
	// Frozen 판정은 FreezeGauge / Health 변경 시 수행.
}

void ADRPlayerState::EvaluateFrozenState(float FreezeGauge, float Health)
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || !FrozenEffectClass)
	{
		return;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Dead))
	{
		return;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Frozen))
	{
		return;
	}

	if (Health <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (FreezeGauge + KINDA_SMALL_NUMBER < Health)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(FrozenEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle FrozenHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	if (!FrozenHandle.IsValid())
	{
		return;
	}

	/*
	 * Frozen 진입 전에 이미 진행 중이던
	 * 원거리 공격 Ability를 즉시 종료한다.
	 */
	FGameplayTagContainer RangedAttackTags;
	RangedAttackTags.AddTag(DRGameplayTags::Ability_Attack_Ranged);

	AbilitySystemComponent->CancelAbilities(&RangedAttackTags);

	AbilitySystemComponent->SetNumericAttributeBase(UDRPlayerAttributeSet::GetFreezeGaugeAttribute(), 0.f);

	StopFreezeDecay();
}

void ADRPlayerState::HandleFreezeGaugeResolved()
{
	if (!HasAuthority()
		|| !IsValid(PlayerAttributeSet))
	{
		return;
	}

	EvaluateFrozenState(
		PlayerAttributeSet->GetFreezeGauge(),
		PlayerAttributeSet->GetHealth());
}

void ADRPlayerState::InitializeDefaultPlayerName()
{
	if (!HasAuthority())
	{
		return;
	}

	const FString CurrentName = GetPlayerName().TrimStartAndEnd();

	/*
	 * 명시적으로 이름이 들어왔다면 유지.
	 * 현재 개발 단계의 자동 PIE 이름도 기본 이름으로 취급하려면
	 * 여기 정책을 추가하면 됨.
	 */
	if (!CurrentName.IsEmpty() && !CurrentName.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		return;
	}

	const int32 NameCount = UE_ARRAY_COUNT(DRPlayerName::AnimalNames);

	const int32 RandomIndex = FMath::RandRange(0, NameCount - 1);

	const FString RandomName = FString::Printf(TEXT("%s %d"), DRPlayerName::AnimalNames[RandomIndex], GetPlayerId());

	SetPlayerName(RandomName);
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
		TEXT("[Coin] Player=%s Previous=%d New=%d Delta=%d"),
		*GetNameSafe(this),
		PreviousCoins,
		Coins,
		Coins - PreviousCoins);

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
	if (ADRPlayerCharacter* PlayerCharacter = GetPawn<ADRPlayerCharacter>())
	{
		PlayerCharacter->RefreshTeamColor();
	}
	for (TActorIterator<ADRPlayerCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Iterator->GetSilhouetteComponent()->RefreshTeamSilhouette();
	}
	ForceNetUpdate();
}

void ADRPlayerState::OnRep_TeamId()
{
	if (ADRPlayerCharacter* PlayerCharacter = GetPawn<ADRPlayerCharacter>())
	{
		PlayerCharacter->RefreshTeamColor();
	}
	for (TActorIterator<ADRPlayerCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		Iterator->GetSilhouetteComponent()->RefreshTeamSilhouette();
	}
}
#pragma endregion
