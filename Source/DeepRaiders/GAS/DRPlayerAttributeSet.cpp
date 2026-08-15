#include "DRPlayerAttributeSet.h"

#include "Net/UnrealNetwork.h"

UDRPlayerAttributeSet::UDRPlayerAttributeSet()
{
	InitSnowGauge(0.f);
	InitMaxSnowGauge(100.f);
}

void UDRPlayerAttributeSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(
		UDRPlayerAttributeSet,
		SnowGauge,
		COND_None,
		REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(
		UDRPlayerAttributeSet,
		MaxSnowGauge,
		COND_None,
		REPNOTIFY_Always);
}

void UDRPlayerAttributeSet::OnRep_SnowGauge(
	const FGameplayAttributeData& OldSnowGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UDRPlayerAttributeSet,
		SnowGauge,
		OldSnowGauge);
	
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[GAS][OnRep_SnowGauge] "
			"Old=%.1f New=%.1f"),
		OldSnowGauge.GetCurrentValue(),
		GetSnowGauge());
}

void UDRPlayerAttributeSet::OnRep_MaxSnowGauge(
	const FGameplayAttributeData& OldMaxSnowGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UDRPlayerAttributeSet,
		MaxSnowGauge,
		OldMaxSnowGauge);
}