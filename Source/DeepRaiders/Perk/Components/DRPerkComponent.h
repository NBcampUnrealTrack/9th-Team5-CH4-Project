#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "GameplayTagContainer.h"
#include "DRPerkComponent.generated.h"

class UAbilitySystemComponent;
class UDRPerkDefinition;
class UDRSkillDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRPerksChangedSignature);

/** 플레이어가 보유한 퍽 하나의 정의와 서버 GAS 적용 상태다. */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRPerkEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk")
	FGuid PerkInstanceId;

	/** 소유 클라이언트에 복제할 퍽 정의다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk")
	TObjectPtr<UDRPerkDefinition> PerkDefinition;

	/** 이 퍽이 장착된 스킬 고유 ID. 비어 있으면 기존 공용 퍽이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk")
	FGameplayTag EquippedSkillId;

	/** 초기화 시 퍽 GameplayEffect를 회수하기 위한 서버 전용 핸들이다. */
	FActiveGameplayEffectHandle EffectHandle;
};

UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRPerkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 플레이어별 퍽 상태를 관리하는 복제 컴포넌트를 초기화한다. */
	UDRPerkComponent();

	/** 보유 퍽 목록을 소유 클라이언트에 복제하도록 등록한다. */
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 보유 중인 동일 퍽 개수를 반환한다. */
	int32 GetPerkCount(const UDRPerkDefinition* PerkDefinition) const;

	/** 현재 유효한 전체 퍽 개수를 반환한다. */
	int32 GetTotalPerkCount() const;

	/** UI에 표시할 현재 퍽 목록을 반환한다. */
	const TArray<FDRPerkEntry>& GetPerkEntries() const
	{
		return PerkEntries;
	}

	/** UI에 표시할 전체 퍽 슬롯 수를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Perk")
	int32 GetMaxPerkSlotCount() const
	{
		return MaxPerkSlotCount;
	}

	/** 전체 퍽 슬롯 제한 안에서 퍽을 추가할 수 있는지 확인한다. */
	bool CanAddPerk(const UDRPerkDefinition* PerkDefinition, FGameplayTag EquippedSkillId = FGameplayTag()) const;

	/** 서버에서 퍽을 추가하고 GameplayEffect를 즉시 적용한다. */
	bool AddPerk(UDRPerkDefinition* PerkDefinition, FGameplayTag EquippedSkillId = FGameplayTag());

	/** 현재 장착된 스킬 정의를 검증해 퍽을 장착한다. */
	bool AddPerkToSkill(UDRPerkDefinition* PerkDefinition, const UDRSkillDefinition* SkillDefinition);

	/** 스킬 사용이 성공했을 때 기본 효과와 장착 퍽의 즉발 효과를 실행한다. 서버 전용. */
	void HandleSkillCommitted(const UDRSkillDefinition* SkillDefinition);

	/**
	 * 스킬 기본 효과와 이 스킬에 실제 장착된 퍽의 활성 중 효과를 적용한다.
	 * 반환된 핸들은 스킬 종료 시 Ability가 회수한다. 서버 전용.
	 */
	void HandleSkillActivated(
		const UDRSkillDefinition* SkillDefinition,
		TArray<FActiveGameplayEffectHandle>& OutActiveEffectHandles);

	/** 특정 스킬에 장착된 설정 변경형 퍽 태그를 반환한다. */
	bool HasSkillPerk(FGameplayTag SkillId, FGameplayTag PerkTag) const;

	/** 고유 ID가 일치하는 퍽의 효과와 슬롯을 함께 제거한다. */
	bool TryRemovePerk(FGuid PerkInstanceId);

	/** 고유 ID가 일치하는 퍽 정의를 반환한다. */
	UDRPerkDefinition* FindPerkDefinition(FGuid PerkInstanceId) const;

	/** 서버에서 모든 퍽 슬롯과 적용된 GameplayEffect를 초기화한다. */
	bool ResetPerks();

	/** 소유 클라이언트에서 서버에 퍽 초기화를 요청한다. */
	void RequestResetPerks();

	UPROPERTY(BlueprintAssignable, Category = "Perk")
	FDRPerksChangedSignature OnPerksChanged;

private:
	/** 표시 가능한 범위 안에서 비어 있는 첫 슬롯을 반환한다. */
	int32 FindAvailableSlotIndex() const;

	/** 고유 ID와 정의가 모두 유효한 퍽의 배열 위치를 반환한다. */
	int32 FindPerkIndex(FGuid PerkInstanceId) const;

	/** 퍽 정의로 GameplayEffectSpec을 생성하고 서버 ASC에 적용한다. */
	FActiveGameplayEffectHandle ApplyPerkEffect(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UDRPerkDefinition* PerkDefinition,
		bool bApplyPersistentPolicy) const;

	FActiveGameplayEffectHandle ApplySkillEffectRule(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UObject* SourceObject,
		const FDRSkillEffectRule& EffectRule,
		bool bPersistThroughDeath) const;

	void ApplySkillEffectRules(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UObject* SourceObject,
		const TArray<FDRSkillEffectRule>& EffectRules,
		EDRSkillEffectTrigger Trigger,
		bool bPersistThroughDeath,
		TArray<FActiveGameplayEffectHandle>* OutActiveEffectHandles) const;

	bool HasUsableSkillEffectRule(const UDRPerkDefinition* PerkDefinition) const;

	/** 소유 클라이언트의 초기화 요청을 서버에서 실행한다. */
	UFUNCTION(Server, Reliable)
	void ServerResetPerks();

	/** 복제된 퍽 목록의 변경을 소유 클라이언트에 알린다. */
	UFUNCTION()
	void OnRep_PerkEntries();

	/** 플레이어가 보유한 퍽과 서버에서 적용한 Effect 핸들을 관리한다. */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_PerkEntries,
		Category = "Perk",
		meta = (AllowPrivateAccess = true))
	TArray<FDRPerkEntry> PerkEntries;

	/** 보유할 수 있는 전체 퍽 개수다. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Perk",
		meta = (AllowPrivateAccess = true, ClampMin = 1, UIMin = 1))
	int32 MaxPerkSlotCount = 5;

};
