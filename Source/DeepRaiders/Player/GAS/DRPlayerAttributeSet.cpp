#include "DRPlayerAttributeSet.h"

#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UDRPlayerAttributeSet::UDRPlayerAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(100.f);

	InitMaxFreezeGauge(100.f);
	InitFreezeGauge(0.f);

	InitMaxSnowGauge(100.f);
	InitSnowGauge(0.f);

	InitMoveSpeedMultiplier(1.f);
}

void UDRPlayerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, FreezeGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MaxFreezeGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, SnowGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRPlayerAttributeSet, MaxSnowGauge, COND_None, REPNOTIFY_Always);
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

void UDRPlayerAttributeSet::OnRep_MaxFreezeGauge(const FGameplayAttributeData& OldMaxFreezeGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRPlayerAttributeSet, MaxFreezeGauge, OldMaxFreezeGauge);
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
	else if (Attribute == GetMaxFreezeGaugeAttribute())
	{
		if (GetFreezeGauge() > NewValue)
		{
			SetFreezeGauge(NewValue);
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
	else if (Attribute == GetMaxFreezeGaugeAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetFreezeGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxFreezeGauge());
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
	else if (Attribute == GetMoveSpeedMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UDRPlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();

		SetIncomingDamage(0.f);

		if (Damage > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.f, GetMaxHealth()));
		}
	
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[GAS][Damage] Damage=%.1f Health=%.1f"),
			Damage,
			GetHealth());
	}
}
