#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRCombatStatsComponent.generated.h"

USTRUCT(BlueprintType)
struct FDRMatchCombatStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat Stats")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Stats")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Stats")
	float DamageDealt = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Stats")
	float DamageTaken = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRCombatStatsChangedSignature, FDRMatchCombatStats, NewStats);

UCLASS(ClassGroup = (Player))
class DEEPRAIDERS_API UDRCombatStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRCombatStatsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Player|Combat Stats")
	FDRMatchCombatStats GetMatchStats() const
	{
		return MatchStats;
	}

	/*
	 * 서버 전용.
	 * 한 Damage Resolve에서 Damage + Kill을 한 번에 갱신한다.
	 */
	void RecordDamageDealt(float AppliedDamage, bool bKill);

	/*
	 * 서버 전용.
	 * 한 Damage Resolve에서 Damage + Death를 한 번에 갱신한다.
	 */
	void RecordDamageTaken(float AppliedDamage, bool bDeath);

	/*
	 * 매치 시작/재시작용.
	 * Respawn에서는 호출하면 안 된다.
	 */
	void ResetMatchStats();

	UPROPERTY(BlueprintAssignable, Category = "Player|Combat Stats")
	FDRCombatStatsChangedSignature OnCombatStatsChanged;

private:
	UPROPERTY(ReplicatedUsing = OnRep_MatchStats)
	FDRMatchCombatStats MatchStats;

	UFUNCTION()
	void OnRep_MatchStats();

	bool HasServerAuthority() const;

	void NotifyStatsChanged();
};
