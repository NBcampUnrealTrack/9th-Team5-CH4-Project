#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Skill/DRSkillTypes.h"
#include "GameplayEffectTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "DRPerkComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
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

	/** 교체형 퍽 판매 시 복구할 원본 스킬이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk")
	TObjectPtr<UDRSkillDefinition> ReplacedSkillDefinition;

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
	bool AddPerk(
		UDRPerkDefinition* PerkDefinition,
		FGameplayTag EquippedSkillId = FGameplayTag(),
		UDRSkillDefinition* ReplacedSkillDefinition = nullptr);

	/** 공용 퍽 또는 유일하게 호환되는 장착 스킬 퍽을 추가할 수 있는지 확인한다. */
	bool CanAddPerkAutomatically(const UDRPerkDefinition* PerkDefinition) const;

	/** 현재 장착한 스킬 중 하나와 호환되거나 공용 퍽인지 확인한다. */
	bool IsCompatibleWithEquippedSkills(const UDRPerkDefinition* PerkDefinition) const;

	/** 공용 퍽 또는 유일하게 호환되는 장착 스킬 퍽을 자동으로 추가한다. */
	bool AddPerkAutomatically(UDRPerkDefinition* PerkDefinition);

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

	/** 스킬이 정상 종료됐을 때 기본 효과와 장착 퍽의 종료 후 효과를 실행한다. 서버 전용. */
	void HandleSkillCompleted(const UDRSkillDefinition* SkillDefinition);

	/** 특정 스킬에 장착된 설정 변경형 퍽 태그를 반환한다. */
	bool HasSkillPerk(FGameplayTag SkillId, FGameplayTag PerkTag) const;

	/** 특정 스킬에 장착된 퍽 규칙의 설정값을 합산해 반환한다. */
	float GetSkillEffectValue(
		FGameplayTag SkillId,
		EDRSkillEffectTrigger Trigger,
		FGameplayTag EffectValueTag) const;

	/** 특정 스킬에 장착된 지정 퍽의 설정값을 반환한다. */
	float GetSkillPerkEffectValue(
		FGameplayTag SkillId,
		FGameplayTag PerkTag,
		EDRSkillEffectTrigger Trigger,
		FGameplayTag EffectValueTag) const;

	/** 장착된 스킬 대상으로 지정된 모든 Rule을 완성된 GameplayEffectSpec으로 생성한다. */
	void BuildEquippedSkillEffectSpecs(
		UAbilitySystemComponent* AbilitySystemComponent,
		FGameplayTag SkillId,
		EDRSkillEffectTrigger Trigger,
		TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;

	/** 소유 캐릭터 대상으로 지정된 Rule의 완성된 GameplayEffectSpec을 생성한다. */
	void BuildOwnerSkillEffectSpecs(
		UAbilitySystemComponent* AbilitySystemComponent,
		FGameplayTag SkillId,
		EDRSkillEffectTrigger Trigger,
		TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;

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
	/** 호환되는 장착 스킬이 정확히 하나일 때 해당 스킬을 반환한다. */
	const UDRSkillDefinition* FindUniqueCompatibleEquippedSkill(
		const UDRPerkDefinition* PerkDefinition) const;

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

	/** EffectRule의 클래스, 출처, 정책 및 모든 SetByCaller 값을 하나의 Spec으로 완성한다. */
	FGameplayEffectSpecHandle BuildSkillEffectRuleSpec(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UObject* SourceObject,
		const FDRSkillEffectRule& EffectRule,
		bool bPersistThroughDeath) const;

	/** 퍽이 선언한 Ability들을 참조 횟수 기반으로 부여한다. */
	bool AcquireGrantedAbilities(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UDRPerkDefinition* PerkDefinition);

	/** 퍽이 선언한 Ability 참조를 반납하고 마지막 참조면 ASC에서 회수한다. */
	void ReleaseGrantedAbilities(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UDRPerkDefinition* PerkDefinition);

	void ApplySkillEffectRules(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UObject* SourceObject,
		const TArray<FDRSkillEffectRule>& EffectRules,
		EDRSkillEffectTrigger Trigger,
		bool bPersistThroughDeath,
		const UDRPerkDefinition* PerkDefinition,
		TArray<FActiveGameplayEffectHandle>* OutActiveEffectHandles) const;

	bool HasUsableSkillEffectRule(const UDRPerkDefinition* PerkDefinition) const;

	bool RestoreReplacedSkill(const FDRPerkEntry& PerkEntry) const;

	/** 차지 퍽 판매 시 다음 한 칸의 충전 시간만 일반 쿨다운으로 보존한다. */
	void NormalizeChargeCooldownOnRemoval(const FDRPerkEntry& PerkEntry) const;

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

	/** 서로 다른 퍽이 같은 Ability를 요구할 때 중복 Spec 생성을 막는 서버 전용 상태다. */
	TMap<TSubclassOf<UGameplayAbility>, int32> GrantedAbilityRefCounts;
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> GrantedAbilityHandles;

};
