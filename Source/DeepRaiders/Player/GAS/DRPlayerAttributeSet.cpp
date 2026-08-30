#include "DRPlayerAttributeSet.h"

#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "DeepRaiders/Player/DRPlayerState.h"

UDRPlayerAttributeSet::UDRPlayerAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(100.f);

	InitFreezeGauge(0.f);

	InitMaxSnowGauge(100.f);
	InitSnowGauge(100.f);

	InitDamageReduction(0.f);
	InitMoveSpeedMultiplier(1.f);
}

void UDRPlayerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, FreezeGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, SnowGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MaxSnowGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, DamageReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void UDRPlayerAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, Health, OldHealth);
}

void UDRPlayerAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, MaxHealth, OldMaxHealth);
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
	else if (Attribute == GetMaxSnowGaugeAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetSnowGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxSnowGauge());
	}
	else if (Attribute == GetIncomingDamageAttribute())
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

void UDRPlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// FreezeGain Instant GE가 실제 적용 완료된 시점에서
	// 확정된 Gauge / Health로 빙결 상태를 평가한다.
	if (Data.EvaluatedData.Attribute == GetFreezeGaugeAttribute())
	{
		UAbilitySystemComponent* TargetASC =
			GetOwningAbilitySystemComponent();

		ADRPlayerState* TargetPlayerState =
			IsValid(TargetASC)
				? Cast<ADRPlayerState>(
					TargetASC->GetOwnerActor())
				: nullptr;

		if (IsValid(TargetPlayerState)
			&& TargetPlayerState->HasAuthority())
		{
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
	const float HealthAfter = FMath::Clamp(HealthBefore - FinalDamage, 0.f, GetMaxHealth());
	const float AppliedDamage = HealthBefore - HealthAfter;

	if (AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
	UAbilitySystemComponent* SourceASC = Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent();

	ADRPlayerState* TargetPlayerState = IsValid(TargetASC) ? Cast<ADRPlayerState>(TargetASC->GetOwnerActor()) : nullptr;
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
