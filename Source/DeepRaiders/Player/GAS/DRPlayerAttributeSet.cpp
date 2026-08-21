#include "DRPlayerAttributeSet.h"

#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "DeepRaiders/Player/DRPlayerState.h"

UDRPlayerAttributeSet::UDRPlayerAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(100.f);

	InitMaxFreezeGauge(100.f);
	InitFreezeGauge(0.f);

	InitMaxSnowGauge(100.f);
	InitSnowGauge(100.f);

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

	const float HealthAfter = FMath::Clamp(HealthBefore - RawDamage, 0.f, GetMaxHealth());
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
	// 먼저 Combat Result를 기록
	if (IsValid(TargetPlayerState) && TargetPlayerState->HasAuthority())
	{
		TargetPlayerState->HandleDamageResolved(SourcePlayerState, AppliedDamage, bFatal);
	}

	SetHealth(HealthAfter);

	UE_LOG(LogTemp, Log, TEXT( "[GAS][Damage] " "Raw=%.1f Applied=%.1f " "Health=%.1f->%.1f Fatal=%d " "Source=%s Target=%s"), 
		RawDamage, AppliedDamage, HealthBefore, HealthAfter, bFatal, *GetNameSafe(SourcePlayerState), *GetNameSafe(TargetPlayerState));
}
