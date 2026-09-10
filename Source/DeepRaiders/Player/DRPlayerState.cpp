#include "DRPlayerState.h"
#include "DeepRaiders/Upgrade/DRCharacterUpgradeComponent.h"

#include "DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRSilhouetteComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/GAS/DRAbilitySystemComponent.h"
#include "DeepRaiders/Player/GAS/DRPlayerAttributeSet.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/Skill/Components/DRSkillComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Player/Components/DRQuickSlotComponent.h"
#include "DeepRaiders/Player/Components//DRCombatStatsComponent.h"
#include "DeepRaiders/Player/Components/DRShieldComponent.h"
#include "DeepRaiders/Input/DRInputTypes.h"
#include "DRPlayerController.h"
#include "HAL/PlatformProcess.h"

ADRPlayerState::ADRPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AbilitySystemComponent->GenericConfirmInputID = static_cast<int32>(EDRAbilityInputId::Primary);
	AbilitySystemComponent->GenericCancelInputID = static_cast<int32>(EDRAbilityInputId::Secondary);

	PlayerAttributeSet = CreateDefaultSubobject<UDRPlayerAttributeSet>(TEXT("PlayerAttributeSet"));
	CharacterUpgradeComponent = CreateDefaultSubobject<UDRCharacterUpgradeComponent>(TEXT("CharacterUpgradeComponent"));
	PerkComponent = CreateDefaultSubobject<UDRPerkComponent>(TEXT("PerkComponent"));
	SkillComponent = CreateDefaultSubobject<UDRSkillComponent>(TEXT("SkillComponent"));
	CombatStatsComponent = CreateDefaultSubobject<UDRCombatStatsComponent>(TEXT("CombatStatsComponent"));
	ShieldComponent = CreateDefaultSubobject<UDRShieldComponent>(TEXT("ShieldComponent"));
}

UAbilitySystemComponent* ADRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UDRAbilitySystemComponent* ADRPlayerState::GetDRAbilitySystemComponent() const
{
	return Cast<UDRAbilitySystemComponent>(AbilitySystemComponent);
}

void ADRPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRPlayerState, bHasDeepestDigLocation);
	DOREPLIFETIME(ADRPlayerState, DeepestDigLocation);
	DOREPLIFETIME(ADRPlayerState, bHasJetpack);
	DOREPLIFETIME_CONDITION(ADRPlayerState, CurrentJetpackFuel, COND_OwnerOnly);

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

	if (bFatal && IsValid(SourcePlayerState) && SourcePlayerState != this)
	{
		UAbilitySystemComponent* SourceASC = SourcePlayerState->GetAbilitySystemComponent();
		if (IsValid(SourceASC))
		{
			AActor* SourceActor = IsValid(SourcePlayerState->GetPawn())
				? static_cast<AActor*>(SourcePlayerState->GetPawn())
				: SourcePlayerState;
			FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
			Context.AddInstigator(SourceActor, SourceActor);

			FGameplayCueParameters Parameters(Context);
			Parameters.Location = SourceActor->GetActorLocation();
			SourceASC->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Sound_Player_Kill, Parameters);
		}
	}

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

	if (IsValid(AbilitySystemComponent))
	{
		AActor* TargetActor = IsValid(GetPawn()) ? static_cast<AActor*>(GetPawn()) : this;
		AActor* SourceActor = IsValid(SourcePlayerState->GetPawn())
			? static_cast<AActor*>(SourcePlayerState->GetPawn())
			: SourcePlayerState;

		FGameplayCueParameters Parameters;
		Parameters.Location = TargetActor->GetActorLocation();
		Parameters.Instigator = SourceActor;
		Parameters.EffectCauser = SourceActor;
		AbilitySystemComponent->ExecuteGameplayCue(DRGameplayTags::GameplayCue_Sound_Player_Hit, Parameters);
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

float ADRPlayerState::GetSnowGauge() const
{
	return IsValid(PlayerAttributeSet) ? PlayerAttributeSet->GetSnowGauge() : 0.f;
}

void ADRPlayerState::AddSnowGauge(float Amount)
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || !IsValid(PlayerAttributeSet))
	{
		return;
	}

	AbilitySystemComponent->ApplyModToAttribute(
		UDRPlayerAttributeSet::GetSnowGaugeAttribute(),
		EGameplayModOp::Additive,
		Amount);
}

void ADRPlayerState::ResetForGameStart()
{
	if (!HasAuthority())
	{
		return;
	}

	ResetHeatState();
	if (IsValid(ShieldComponent))
	{
		ShieldComponent->ClearShieldLayers();
	}
	if (IsValid(AbilitySystemComponent))
	{
		FGameplayTagContainer ShieldTags;
		ShieldTags.AddTag(DRGameplayTags::State_PersonalShield);
		AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(ShieldTags);
		AbilitySystemComponent->SetNumericAttributeBase(
			UDRPlayerAttributeSet::GetShieldAttribute(),
			0.f);
	}
	if (IsValid(PerkComponent))
	{
		PerkComponent->ResetPerks();
	}
	if (IsValid(SkillComponent))
	{
		SkillComponent->ResetSkills();
	}
	CharacterUpgradeComponent->ResetUpgrades();
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
	ResetHeatState();
	if (IsValid(ShieldComponent))
	{
		ShieldComponent->ClearShieldLayers();
	}
	
	FGameplayTagContainer PersistThroughDeathTags;
	PersistThroughDeathTags.AddTag(DRGameplayTags::Effect_Policy_PersistThroughDeath);
	
	const FGameplayEffectQuery RemoveOnRespawnQuery = FGameplayEffectQuery::MakeQuery_MatchNoEffectTags(PersistThroughDeathTags);
	AbilitySystemComponent->RemoveActiveEffects(RemoveOnRespawnQuery);

	// Respawn Attribute 초기화
	AbilitySystemComponent->SetNumericAttributeBase(UDRPlayerAttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth());
	AbilitySystemComponent->SetNumericAttributeBase(UDRPlayerAttributeSet::GetShieldAttribute(), 0.f);
}

bool ADRPlayerState::IsFrozen() const
{
	return IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Frozen);
}

bool ADRPlayerState::IsOverheated() const
{
	return IsValid(AbilitySystemComponent) &&
		AbilitySystemComponent->HasMatchingGameplayTag(DRGameplayTags::State_Overheated);
}

void ADRPlayerState::AddWeaponHeat(
	float HeatAmount,
	float DecayDelay,
	float RecoveryDuration)
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || HeatAmount <= 0.f)
	{
		return;
	}

	const UDRPlayerAttributeSet* Attributes =
		AbilitySystemComponent->GetSet<UDRPlayerAttributeSet>();
	if (!IsValid(Attributes) || Attributes->GetMaxHeatGauge() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 마지막으로 Heat를 발생시킨 무기의 냉각 정책을 고정한다.
	// 무기만 교체해서 더 짧은 냉각 시간을 얻는 우회를 막는다.
	CurrentHeatDecayDelay = FMath::Max(0.f, DecayDelay);
	const float SafeRecoveryDuration = FMath::Max(0.01f, RecoveryDuration);
	CurrentHeatDecayRatePerSecond = Attributes->GetMaxHeatGauge() / SafeRecoveryDuration;

	const FGameplayAttribute HeatAttribute = UDRPlayerAttributeSet::GetHeatGaugeAttribute();
	const float CurrentHeat = AbilitySystemComponent->GetNumericAttribute(HeatAttribute);
	const float NewHeat = FMath::Min(
		Attributes->GetMaxHeatGauge(),
		CurrentHeat + HeatAmount);
	AbilitySystemComponent->SetNumericAttributeBase(HeatAttribute, NewHeat);
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
	BindStatusPolicy();

	if (HasAuthority())
	{
		EvaluateDeadState();
		EvaluateFrozenState(PlayerAttributeSet->GetFreezeGauge(), PlayerAttributeSet->GetHealth());
		EvaluateOverheatedState(PlayerAttributeSet->GetHeatGauge());
	}
}

void ADRPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFreezeDecay();
	StopHeatDecay();
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
	StopHeatDecay();

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

	HeatGaugeChangedHandle =
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::GetHeatGaugeAttribute())
			.AddUObject(
				this,
				&ThisClass::HandleHeatGaugeChanged);

	HealthChangedHandle =
		AbilitySystemComponent->
			GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::
					GetHealthAttribute())
			.AddUObject(
				this,
				&ThisClass::HandleHealthChanged);

	FrozenTagChangedHandle =
		AbilitySystemComponent->RegisterGameplayTagEvent(
			DRGameplayTags::State_Frozen,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(
			this,
			&ThisClass::HandleFrozenTagChanged);

	SetFrozenAbilityBlockActive(
		AbilitySystemComponent->HasMatchingGameplayTag(
			DRGameplayTags::State_Frozen));

	VoxelContainedTagChangedHandle =
		AbilitySystemComponent->RegisterGameplayTagEvent(
			DRGameplayTags::State_VoxelContained,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(
			this,
			&ThisClass::HandleVoxelContainedTagChanged);

	PersonalShieldTagChangedHandle =
		AbilitySystemComponent->RegisterGameplayTagEvent(
			DRGameplayTags::State_PersonalShield,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(
			this,
			&ThisClass::HandlePersonalShieldTagChanged);
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

	if (HeatGaugeChangedHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				UDRPlayerAttributeSet::GetHeatGaugeAttribute())
			.Remove(HeatGaugeChangedHandle);
		HeatGaugeChangedHandle.Reset();
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

	if (FrozenTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			DRGameplayTags::State_Frozen,
			EGameplayTagEventType::NewOrRemoved)
		.Remove(FrozenTagChangedHandle);
		FrozenTagChangedHandle.Reset();
	}

	SetFrozenAbilityBlockActive(false);

	if (VoxelContainedTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			DRGameplayTags::State_VoxelContained,
			EGameplayTagEventType::NewOrRemoved)
		.Remove(VoxelContainedTagChangedHandle);
		VoxelContainedTagChangedHandle.Reset();
	}

	if (PersonalShieldTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			DRGameplayTags::State_PersonalShield,
			EGameplayTagEventType::NewOrRemoved)
		.Remove(PersonalShieldTagChangedHandle);
		PersonalShieldTagChangedHandle.Reset();
	}
}

void ADRPlayerState::HandleFrozenTagChanged(
	const FGameplayTag CallbackTag,
	int32 NewCount)
{
	SetFrozenAbilityBlockActive(NewCount > 0);
}

void ADRPlayerState::SetFrozenAbilityBlockActive(bool bActive)
{
	if (!IsValid(AbilitySystemComponent)
		|| bFrozenAbilityBlockApplied == bActive)
	{
		return;
	}

	FGameplayTagContainer ActionAbilityTags;
	ActionAbilityTags.AddTag(DRGameplayTags::Ability_Action);

	if (bActive)
	{
		bFrozenAbilityBlockApplied = true;
		AbilitySystemComponent->BlockAbilitiesWithTags(ActionAbilityTags);
		AbilitySystemComponent->CancelAbilities(&ActionAbilityTags);
		return;
	}

	AbilitySystemComponent->UnBlockAbilitiesWithTags(ActionAbilityTags);
	bFrozenAbilityBlockApplied = false;
}

void ADRPlayerState::HandlePersonalShieldTagChanged(
	const FGameplayTag CallbackTag,
	int32 NewCount)
{
	if (!HasAuthority()
		|| NewCount > 0
		|| !IsValid(AbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent->SetNumericAttributeBase(
		UDRPlayerAttributeSet::GetShieldAttribute(),
		0.f);
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

void ADRPlayerState::HandleHeatGaugeChanged(const FOnAttributeChangeData& Data)
{
	if (!HasAuthority())
	{
		return;
	}

	EvaluateOverheatedState(Data.NewValue);

	if (Data.NewValue > Data.OldValue + KINDA_SMALL_NUMBER)
	{
		RestartHeatDecay();
		return;
	}

	if (Data.NewValue <= KINDA_SMALL_NUMBER)
	{
		StopHeatDecay();
		ClearOverheatedState();
	}
}

void ADRPlayerState::EvaluateOverheatedState(float HeatGauge)
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || !IsValid(PlayerAttributeSet))
	{
		return;
	}

	if (HeatGauge + KINDA_SMALL_NUMBER >= PlayerAttributeSet->GetMaxHeatGauge())
	{
		EnterOverheatedState();
	}
	else if (HeatGauge <= KINDA_SMALL_NUMBER)
	{
		ClearOverheatedState();
	}
}

void ADRPlayerState::EnterOverheatedState()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || IsOverheated() || !OverheatedEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		OverheatedEffectClass,
		1.f,
		Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	OverheatedEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	if (!OverheatedEffectHandle.IsValid())
	{
		return;
	}

	// 상태 진입 순간 이미 실행 중인 행동만 정리한다.
	// 신규 활성화 차단 정책은 각 GA BP의 ActivationBlockedTags가 소유한다.
	FGameplayTagContainer AbilitiesToCancel;
	AbilitiesToCancel.AddTag(DRGameplayTags::Ability_Attack_Ranged);
	AbilitiesToCancel.AddTag(DRGameplayTags::Ability_Snow_Absorb);
	AbilitySystemComponent->CancelAbilities(&AbilitiesToCancel);
}

void ADRPlayerState::ClearOverheatedState()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	if (OverheatedEffectHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(OverheatedEffectHandle);
		OverheatedEffectHandle.Invalidate();
	}
}

void ADRPlayerState::ResetHeatState()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	StopHeatDecay();
	ClearOverheatedState();
	AbilitySystemComponent->SetNumericAttributeBase(
		UDRPlayerAttributeSet::GetHeatGaugeAttribute(),
		0.f);
}

void ADRPlayerState::RestartHeatDecay()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	const float CurrentHeat = AbilitySystemComponent->GetNumericAttribute(
		UDRPlayerAttributeSet::GetHeatGaugeAttribute());
	if (CurrentHeat <= KINDA_SMALL_NUMBER || HeatDecayInterval <= 0.f ||
		CurrentHeatDecayRatePerSecond <= 0.f)
	{
		StopHeatDecay();
		return;
	}

	GetWorldTimerManager().SetTimer(
		HeatDecayTimerHandle,
		this,
		&ThisClass::TickHeatDecay,
		HeatDecayInterval,
		true,
		CurrentHeatDecayDelay);
}

void ADRPlayerState::TickHeatDecay()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent))
	{
		StopHeatDecay();
		return;
	}

	const FGameplayAttribute HeatAttribute = UDRPlayerAttributeSet::GetHeatGaugeAttribute();
	const float CurrentHeat = AbilitySystemComponent->GetNumericAttribute(HeatAttribute);
	if (CurrentHeat <= KINDA_SMALL_NUMBER)
	{
		StopHeatDecay();
		ClearOverheatedState();
		return;
	}

	const float DecayAmount = CurrentHeatDecayRatePerSecond * HeatDecayInterval;
	const float NewHeat = FMath::Max(0.f, CurrentHeat - DecayAmount);
	AbilitySystemComponent->SetNumericAttributeBase(HeatAttribute, NewHeat);
}

void ADRPlayerState::StopHeatDecay()
{
	GetWorldTimerManager().ClearTimer(HeatDecayTimerHandle);
}

void ADRPlayerState::HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData&)
{
	// 비주얼용 MaxFreezeGauge 변경.
	// Frozen 판정은 FreezeGauge / Health 변경 시 수행.
}

void ADRPlayerState::HandleVoxelContainedTagChanged(
	const FGameplayTag CallbackTag,
	int32 NewCount)
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	if (NewCount > 0)
	{
		StopFreezeDecay();
		return;
	}

	if (!IsFrozen() &&
		AbilitySystemComponent->GetNumericAttribute(
			UDRPlayerAttributeSet::GetFreezeGaugeAttribute()) >
		KINDA_SMALL_NUMBER)
	{
		RestartFreezeDecay();
	}
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

	AbilitySystemComponent->SetNumericAttributeBase(UDRPlayerAttributeSet::GetFreezeGaugeAttribute(), 0.f);

	StopFreezeDecay();

	// 매몰은 게이지가 한계에 도달한 즉시 기존 DeadEffect 경로로 사망한다.
	// 일반 빙결은 State.VoxelContained가 없으므로 기존 Frozen 상태만 유지한다.
	if (AbilitySystemComponent->HasMatchingGameplayTag(
		DRGameplayTags::State_VoxelContained))
	{
		AbilitySystemComponent->SetNumericAttributeBase(
			UDRPlayerAttributeSet::GetHealthAttribute(),
			0.f);
	}
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

FText ADRPlayerState::GetDisplayPlayerName() const
{
	const FString PlayerName = GetPlayerName().TrimStartAndEnd();

	if (PlayerName.IsEmpty() || PlayerName.StartsWith(TEXT("DESKTOP-"), ESearchCase::IgnoreCase))
	{
		return FText::FromString(TEXT("Player"));
	}

	return FText::FromString(PlayerName);
}

void ADRPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();

	OnPlayerIdentityChanged.Broadcast();
}

void ADRPlayerState::RestartFreezeDecay()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || IsFrozen() ||
	AbilitySystemComponent->HasMatchingGameplayTag(
		DRGameplayTags::State_VoxelContained))
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
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || IsFrozen() ||
	AbilitySystemComponent->HasMatchingGameplayTag(
		DRGameplayTags::State_VoxelContained))
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
