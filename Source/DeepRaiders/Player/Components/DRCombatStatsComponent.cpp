#include "DRCombatStatsComponent.h"

#include "Net/UnrealNetwork.h"

UDRCombatStatsComponent::UDRCombatStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UDRCombatStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/*
	 * Scoreboard에서 모든 플레이어의 Stats가 필요하므로
	 * OwnerOnly가 아니다.
	 */
	DOREPLIFETIME(UDRCombatStatsComponent, MatchStats);
}

void UDRCombatStatsComponent::RecordDamageDealt(float AppliedDamage, bool bKill)
{
	if (!HasServerAuthority() || AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	MatchStats.DamageDealt += AppliedDamage;

	if (bKill)
	{
		++MatchStats.Kills;
	}

	NotifyStatsChanged();
}

void UDRCombatStatsComponent::RecordDamageTaken(float AppliedDamage, bool bDeath)
{
	if (!HasServerAuthority() || AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	MatchStats.DamageTaken += AppliedDamage;

	if (bDeath)
	{
		++MatchStats.Deaths;
	}

	NotifyStatsChanged();
}

void UDRCombatStatsComponent::ResetMatchStats()
{
	if (!HasServerAuthority())
	{
		return;
	}

	MatchStats = FDRMatchCombatStats{};

	NotifyStatsChanged();
}

void UDRCombatStatsComponent::OnRep_MatchStats()
{
	OnCombatStatsChanged.Broadcast(MatchStats);
}

bool UDRCombatStatsComponent::HasServerAuthority() const
{
	const AActor* Owner = GetOwner();

	return IsValid(Owner) && Owner->HasAuthority();
}

void UDRCombatStatsComponent::NotifyStatsChanged()
{
	/*
	 * Dedicated Server에는 UI가 없지만,
	 * Server-side listener가 붙어도 동일한 변경 이벤트를 받을 수 있다.
	 */
	OnCombatStatsChanged.Broadcast(MatchStats);

	/*
	 * PlayerState 복제 갱신을 바로 유도.
	 * 3v3 규모에서는 이 비용은 문제될 수준이 아니다.
	 */
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}
