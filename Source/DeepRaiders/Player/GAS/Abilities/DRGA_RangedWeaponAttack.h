#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DeepRaiders/GAS/DRGameplayEffectData.h"
#include "DRGA_RangedWeaponAttack.generated.h"

class UAbilitySystemComponent;
class UDRInventoryComponent;
class UDRProjectileWeaponItemDefinition;
struct FCollisionQueryParams;
struct FDRItemInstance;

UCLASS(Abstract)
class DEEPRAIDERS_API UDRGA_RangedWeaponAttack : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UDRGA_RangedWeaponAttack();
	
protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	
	// 자식 GA 전용 설정이 유효한지 검사
	// Projectile Class 등
	virtual bool IsAttackConfigurationValid() const;
	
	// 자식 GA가 서버 delegate 등을 등록
	virtual void OnRangedWeaponActivated() {};
	
	// 자식 GA가 등록 한 delegate 등을 해제
	virtual void OnRangedWeaponEnded() {};
	
	/* 로컬 플레이어의 발사 요청 처리
	 * Projectile은 서버 발사 이벤트를 전달
	 * HitScan은 TargetData를 생성해 전달
	 */
	virtual bool SendLocalShotRequest();
	
	// 로컬 Cooldown과 Cost를 검사한 뒤 자식 GA에 발사를 요청
	// 원격 클라이언트는 성공한 요청과 같은 Prediction Key로 Cooldown GE를 예측 적용
	void TryRequestLocalShot();
	
	/*
	 * 서버에서 선택 아이템을 검증하고 CommitAbility로 Cooldown과 Cost를 확정한다.
	 * TargetData 검증처럼 공격 방식별 검사는 호출 전에 자식 GA가 수행
	 */
	bool TryCommitServerShot();
	
	bool GetViewPoint(FVector& OutViewLocation, FRotator& OutViewRotation) const;
	
	bool TraceCameraAim(const FVector& ViewLocation, const FVector& ViewDirection, FHitResult& OutHitResult) const;
	
	bool ResolveGameplayFireOrigin(const FVector& AimDirection, FVector& OutFireOrigin) const;
	
	void BuildWeaponTraceQueryParams(FCollisionQueryParams& OutQueryParams) const;
	
	bool IsFriendlyTarget(const AActor* TargetActor) const;
	int32 GetSourceTeamId() const;
	
	void BuildImpactEffectSpecs(
		TArray<FGameplayEffectSpecHandle>& OutEffectSpecs) const;

	float GetBreakableDamageAmount() const;
	const UDRProjectileWeaponItemDefinition* GetCurrentWeaponDefinition() const;
	
	bool TryApplyBreakableDamage(const FHitResult& HitResult) const;
	
	void ApplyImpactEffectSpecs(
		UAbilitySystemComponent* TargetAbilitySystem,
		const FHitResult& HitResult,
		const TArray<FGameplayEffectSpecHandle>& ImpactEffectSpecs) const;

	void PlayLocalFirePresentation(
		const FVector& FireOrigin,
		const FVector& TargetLocation);

	void PlayServerFirePresentation(
		const FVector& FireOrigin,
		const FVector& TargetLocation);

	float GetMaxAttackDistance() const
	{
		return MaxAttackDistance;
	}

	UPROPERTY(EditDefaultsOnly,	BlueprintReadOnly,Category = "Ranged Weapon|Fire",meta = (
		ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float BaseFireInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Fire")
	bool bAutomaticFire = false;

	UPROPERTY(EditDefaultsOnly,	BlueprintReadOnly,Category = "Ranged Weapon|Aim",meta = (
		ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float MaxAttackDistance = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Effect")
	TArray<FDRGameplayEffectData> ImpactEffects;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged Weapon|Effect", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BreakableDamage = 1.f;

private:
	bool ResolveSelectedWeaponInstance(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UDRProjectileWeaponItemDefinition* ExpectedDefinition,
		UDRInventoryComponent*& OutInventory,
		const FDRItemInstance*& OutItemInstance) const;

	const UDRProjectileWeaponItemDefinition* GetWeaponDefinition(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	void ExecuteFireGameplayCue(const FVector& FireOrigin) const;

	void PlayFireMontage();
};
