#include "DRPlayerAttributeSet.h"

#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "DeepRaiders/Player/DRPlayerState.h"
#include "DeepRaiders/Player/DRPlayerCharacter.h"
#include "DeepRaiders/Player/Components/DRShieldComponent.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

UDRPlayerAttributeSet::UDRPlayerAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(100.f);
	InitShield(0.f);
	InitIncomingShield(0.f);
	InitIncomingKnockbackDistance(0.f);

	InitFreezeGauge(0.f);

	InitMaxSnowGauge(10000000.f); // 일단 Max Snow 1000만으로 설정
	InitSnowGauge(200.0f);

	InitMaxHeatGauge(100.f);
	InitHeatGauge(0.f);

	InitDamageReduction(0.f);
	InitMoveSpeedMultiplier(1.f);

	InitWeaponDamageMultiplier(1.f);
	InitWeaponFireIntervalMultiplier(1.f);
	InitWeaponSnowCostMultiplier(1.f);
	InitWeaponProjectileCountMultiplier(1.f);
	InitWeaponHeatGenerationMultiplier(1.f);
	InitWeaponSnowAbsorbPowerMultiplier(1.f);
	InitWeaponSnowAddAmountMultiplier(1.f);
}

void UDRPlayerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, FreezeGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, SnowGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MaxSnowGauge, COND_None, REPNOTIFY_Always);
	// Heat는 자신의 HUD/입력 판정에만 필요하므로 소유 클라이언트에만 보낸다.
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, HeatGauge, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MaxHeatGauge, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, DamageReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, WeaponDamageMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, WeaponFireIntervalMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, WeaponSnowCostMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, WeaponProjectileCountMultiplier, COND_OwnerOnly,
		REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, WeaponHeatGenerationMultiplier, COND_OwnerOnly,
		REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, WeaponSnowAbsorbPowerMultiplier, COND_OwnerOnly,
		REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, WeaponSnowAddAmountMultiplier, COND_OwnerOnly,
		REPNOTIFY_Always);
}

void UDRPlayerAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, Health, OldHealth);
}

void UDRPlayerAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, MaxHealth, OldMaxHealth);
}

void UDRPlayerAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, Shield, OldShield);
}

void UDRPlayerAttributeSet::OnRep_FreezeGauge(const FGameplayAttributeData& OldFreezeGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, FreezeGauge, OldFreezeGauge);
}

void UDRPlayerAttributeSet::OnRep_SnowGauge(const FGameplayAttributeData& OldSnowGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, SnowGauge, OldSnowGauge);
}

void UDRPlayerAttributeSet::OnRep_MaxSnowGauge(const FGameplayAttributeData& OldMaxSnowGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, MaxSnowGauge, OldMaxSnowGauge);
}

void UDRPlayerAttributeSet::OnRep_HeatGauge(const FGameplayAttributeData& OldHeatGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, HeatGauge, OldHeatGauge);
}

void UDRPlayerAttributeSet::OnRep_MaxHeatGauge(const FGameplayAttributeData& OldMaxHeatGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, MaxHeatGauge, OldMaxHeatGauge);
}

void UDRPlayerAttributeSet::OnRep_MoveSpeedMultiplier(
	const FGameplayAttributeData& OldMoveSpeedMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UDRPlayerAttributeSet,
		MoveSpeedMultiplier,
		OldMoveSpeedMultiplier);
}

void UDRPlayerAttributeSet::OnRep_DamageReduction(
	const FGameplayAttributeData& OldDamageReduction)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UDRPlayerAttributeSet,
		DamageReduction,
		OldDamageReduction);
}

void UDRPlayerAttributeSet::OnRep_WeaponDamageMultiplier(
	const FGameplayAttributeData& OldWeaponDamageMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, WeaponDamageMultiplier, OldWeaponDamageMultiplier);
}

void UDRPlayerAttributeSet::OnRep_WeaponFireIntervalMultiplier(
	const FGameplayAttributeData& OldWeaponFireIntervalMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, WeaponFireIntervalMultiplier, OldWeaponFireIntervalMultiplier);
}

void UDRPlayerAttributeSet::OnRep_WeaponSnowCostMultiplier(
	const FGameplayAttributeData& OldWeaponSnowCostMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, WeaponSnowCostMultiplier, OldWeaponSnowCostMultiplier);
}

void UDRPlayerAttributeSet::OnRep_WeaponProjectileCountMultiplier(
	const FGameplayAttributeData& OldWeaponProjectileCountMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, WeaponProjectileCountMultiplier,
		OldWeaponProjectileCountMultiplier);
}

void UDRPlayerAttributeSet::OnRep_WeaponHeatGenerationMultiplier(
	const FGameplayAttributeData& OldWeaponHeatGenerationMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, WeaponHeatGenerationMultiplier,
		OldWeaponHeatGenerationMultiplier);
}

void UDRPlayerAttributeSet::OnRep_WeaponSnowAbsorbPowerMultiplier(
	const FGameplayAttributeData& OldWeaponSnowAbsorbPowerMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, WeaponSnowAbsorbPowerMultiplier,
		OldWeaponSnowAbsorbPowerMultiplier);
}

void UDRPlayerAttributeSet::OnRep_WeaponSnowAddAmountMultiplier(
	const FGameplayAttributeData& OldWeaponSnowAddAmountMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, WeaponSnowAddAmountMultiplier,
		OldWeaponSnowAddAmountMultiplier);
}

void UDRPlayerAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttributeValue(Attribute, NewValue);
}

void UDRPlayerAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttributeValue(Attribute, NewValue);
}

void UDRPlayerAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		if (GetHealth() > NewValue)
		{
			SetHealth(NewValue);
		}
	}
	else if (Attribute == GetMaxSnowGaugeAttribute())
	{
		if (GetSnowGauge() > NewValue)
		{
			SetSnowGauge(NewValue);
		}
	}
	else if (Attribute == GetMaxHeatGaugeAttribute())
	{
		if (GetHeatGauge() > NewValue)
		{
			SetHeatGauge(NewValue);
		}
	}
}

void UDRPlayerAttributeSet::ClampAttributeValue(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute()
		|| Attribute == GetIncomingShieldAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMaxSnowGaugeAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetSnowGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxSnowGauge());
	}
	else if (Attribute == GetMaxHeatGaugeAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetHeatGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHeatGauge());
	}
	else if (Attribute == GetIncomingDamageAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetIncomingKnockbackDistanceAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetDamageReductionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 0.95f);
	}
	else if (Attribute == GetMoveSpeedMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

bool UDRPlayerAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
	if (IsValid(TargetASC)
		&& TargetASC->HasMatchingGameplayTag(DRGameplayTags::State_RespawnInvincible)
		&& ((Data.EvaluatedData.Attribute == GetIncomingDamageAttribute()
				&& Data.EvaluatedData.Magnitude > KINDA_SMALL_NUMBER)
			|| (Data.EvaluatedData.Attribute == GetFreezeGaugeAttribute()
				&& Data.EvaluatedData.Magnitude > KINDA_SMALL_NUMBER)
			|| (Data.EvaluatedData.Attribute == GetIncomingKnockbackDistanceAttribute()
				&& Data.EvaluatedData.Magnitude > KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	/*
	 * 일반 분사 공격은 Health Damage 대신 FreezeGauge를 직접 증가시킨다.
	 * 양수 빙결 누적도 공격으로 간주하여 개인 쉴드가 먼저 흡수하고,
	 * 쉴드를 초과한 값만 실제 FreezeGauge에 적용한다.
	 */
	if (Data.EvaluatedData.Attribute != GetFreezeGaugeAttribute()
		|| Data.EvaluatedData.Magnitude <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const float ShieldBefore = GetShield();
	if (ShieldBefore <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	ADRPlayerState* TargetPlayerState = IsValid(TargetASC)
		? Cast<ADRPlayerState>(TargetASC->GetOwnerActor())
		: nullptr;
	UDRShieldComponent* ShieldComponent = IsValid(TargetPlayerState)
		? TargetPlayerState->GetShieldComponent()
		: nullptr;
	if (IsValid(ShieldComponent) && ShieldComponent->HasShieldLayers())
	{
		Data.EvaluatedData.Magnitude = ShieldComponent->AbsorbDamage(
			Data.EvaluatedData.Magnitude);
	}
	else
	{
		const float AbsorbedFreeze = FMath::Min(
			ShieldBefore,
			Data.EvaluatedData.Magnitude);
		const float ShieldAfter = ShieldBefore - AbsorbedFreeze;
		Data.EvaluatedData.Magnitude -= AbsorbedFreeze;
		SetShield(ShieldAfter);

		if (ShieldAfter <= KINDA_SMALL_NUMBER && IsValid(TargetASC))
		{
			FGameplayTagContainer ShieldTags;
			ShieldTags.AddTag(DRGameplayTags::State_PersonalShield);
			TargetASC->RemoveActiveEffectsWithGrantedTags(ShieldTags);
		}
	}

	if (Data.EvaluatedData.Magnitude > KINDA_SMALL_NUMBER)
	{
		return true;
	}

	// 완전히 흡수되면 FreezeGauge Modifier 자체를 실행하지 않는다.
	// PostGameplayEffectExecute가 호출되지 않으므로 유효 피격 기록은 여기서 처리한다.
	UAbilitySystemComponent* SourceASC =
		Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent();
	ADRPlayerState* SourcePlayerState = IsValid(SourceASC)
		? Cast<ADRPlayerState>(SourceASC->GetOwnerActor())
		: nullptr;
	if (IsValid(TargetPlayerState) && TargetPlayerState->HasAuthority())
	{
		TargetPlayerState->HandleHostileHitResolved(SourcePlayerState);
	}

	return false;
}

void UDRPlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingKnockbackDistanceAttribute())
	{
		const float KnockbackDistance = GetIncomingKnockbackDistance();
		SetIncomingKnockbackDistance(0.f);

		const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetContext();
		UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
		ADRPlayerCharacter* TargetCharacter = IsValid(TargetASC)
			? Cast<ADRPlayerCharacter>(TargetASC->GetAvatarActor())
			: nullptr;

		if (KnockbackDistance <= KINDA_SMALL_NUMBER
			|| !EffectContext.HasOrigin()
			|| !IsValid(TargetCharacter)
			|| !TargetCharacter->HasAuthority())
		{
			return;
		}

		TargetCharacter->ApplyKnockback(EffectContext.GetOrigin(), KnockbackDistance);
		return;
	}

	if (Data.EvaluatedData.Attribute == GetIncomingShieldAttribute())
	{
		const float GrantedShield = GetIncomingShield();
		SetIncomingShield(0.f);
		UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
		ADRPlayerState* TargetPlayerState = IsValid(TargetASC)
			? Cast<ADRPlayerState>(TargetASC->GetOwnerActor())
			: nullptr;
		UDRShieldComponent* ShieldComponent = IsValid(TargetPlayerState)
			? TargetPlayerState->GetShieldComponent()
			: nullptr;
		const UObject* SourceObject = Data.EffectSpec.GetContext().GetSourceObject();
		if (IsValid(ShieldComponent)
			&& ShieldComponent->RegisterPersonalShieldGrant(GrantedShield, SourceObject))
		{
			return;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[ShieldLayer] IncomingShield was not registered. Owner=%s Source=%s Amount=%.1f"),
			*GetNameSafe(TargetPlayerState), *GetNameSafe(SourceObject), GrantedShield);
		return;
	}

	// FreezeGain Instant GE가 실제 적용 완료된 시점에서
	// 확정된 Gauge / Health로 빙결 상태를 평가한다.
	if (Data.EvaluatedData.Attribute == GetFreezeGaugeAttribute())
	{
		UAbilitySystemComponent* TargetASC =
			GetOwningAbilitySystemComponent();

		UAbilitySystemComponent* SourceASC =
			Data.EffectSpec
				.GetContext()
				.GetOriginalInstigatorAbilitySystemComponent();

		ADRPlayerState* TargetPlayerState =
			IsValid(TargetASC)
				? Cast<ADRPlayerState>(
					TargetASC->GetOwnerActor())
				: nullptr;

		ADRPlayerState* SourcePlayerState =
			IsValid(SourceASC)
				? Cast<ADRPlayerState>(
					SourceASC->GetOwnerActor())
				: nullptr;

		if (IsValid(TargetPlayerState)
			&& TargetPlayerState->HasAuthority())
		{
			/*
			 * 양수 FreezeGauge Modifier가 실제 실행된 경우
			 * 서버 확정 적중으로 취급한다.
			 */
			if (Data.EvaluatedData.Magnitude
				> KINDA_SMALL_NUMBER)
			{
				TargetPlayerState->HandleHostileHitResolved(
					SourcePlayerState);
			}

			TargetPlayerState->HandleFreezeGaugeResolved();
		}

		return;
	}
	
	if (Data.EvaluatedData.Attribute != GetIncomingDamageAttribute())
	{
		return;
	}

	const float RawDamage = GetIncomingDamage();
	// 처리 직후 바로 비움
	SetIncomingDamage(0.f);
	if (RawDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float HealthBefore = GetHealth();
	// 이미 죽은 대상에게 들어온 후속 Effect는 통계에 포함하지 않는다.
	if (HealthBefore <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float FinalDamageReduction = GetDamageReduction();
	const float FinalDamage = RawDamage * (1.f - FinalDamageReduction);
	const float ShieldBefore = GetShield();
	UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
	ADRPlayerState* TargetPlayerState = IsValid(TargetASC)
		? Cast<ADRPlayerState>(TargetASC->GetOwnerActor())
		: nullptr;
	UDRShieldComponent* ShieldComponent = IsValid(TargetPlayerState)
		? TargetPlayerState->GetShieldComponent()
		: nullptr;
	const bool bUsesShieldLayers = IsValid(ShieldComponent)
		&& ShieldComponent->HasShieldLayers();
	const float HealthDamage = bUsesShieldLayers
		? ShieldComponent->AbsorbDamage(FinalDamage)
		: FMath::Max(0.f, FinalDamage - ShieldBefore);
	const float ShieldDamage = FinalDamage - HealthDamage;
	const float ShieldAfter = ShieldBefore - ShieldDamage;
	const float HealthAfter = FMath::Clamp(
		HealthBefore - HealthDamage,
		0.f,
		GetMaxHealth());
	const float AppliedHealthDamage = HealthBefore - HealthAfter;
	const float AppliedDamage = ShieldDamage + AppliedHealthDamage;

	if (AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (!bUsesShieldLayers)
	{
		SetShield(ShieldAfter);
		if (ShieldBefore > KINDA_SMALL_NUMBER
			&& ShieldAfter <= KINDA_SMALL_NUMBER
			&& IsValid(TargetASC))
		{
			FGameplayTagContainer ShieldTags;
			ShieldTags.AddTag(DRGameplayTags::State_PersonalShield);
			TargetASC->RemoveActiveEffectsWithGrantedTags(ShieldTags);
		}
	}
	UAbilitySystemComponent* SourceASC = Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent();

	ADRPlayerState* SourcePlayerState = IsValid(SourceASC) ? Cast<ADRPlayerState>(SourceASC->GetOwnerActor()) : nullptr;

	const bool bFatal = HealthBefore > KINDA_SMALL_NUMBER && HealthAfter <= KINDA_SMALL_NUMBER;

	// CombatStats
	if (IsValid(TargetPlayerState) && TargetPlayerState->HasAuthority())
	{
		TargetPlayerState->HandleDamageResolved(SourcePlayerState, AppliedDamage, bFatal);
	}

	/*
	 * Frozen 판정 기준이 FreezeGauge >= Current Health이므로,
	 * Health Damage만으로 Freeze 임계값이 내려가
	 * 갑자기 Frozen되는 것을 방지한다.
	 *
	 * Health 감소율만큼 FreezeGauge도 비례 감소시켜
	 * 현재 Freeze 진행 비율을 유지한다.
	 */
	const float FreezeBefore = GetFreezeGauge();

	if (FreezeBefore > KINDA_SMALL_NUMBER && HealthBefore > KINDA_SMALL_NUMBER)
	{
		const float RemainingHealthRatio = HealthAfter / HealthBefore;
		const float FreezeAfter = FreezeBefore * RemainingHealthRatio;

		SetFreezeGauge(FreezeAfter);
	}

	// 반드시 FreezeGauge 조정 이후 Health 변경.
	SetHealth(HealthAfter);
	
}
