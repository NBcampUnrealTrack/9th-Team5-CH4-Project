#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectKey.h"
#include "DRShieldComponent.generated.h"

struct FGameplayEffectSpec;
class UAbilitySystemComponent;
class UGameplayEffect;

/** 하나의 지속 GameplayEffect에 대응하는 독립 실드 레이어다. */
USTRUCT()
struct FDRShieldLayer
{
	GENERATED_BODY()

	FActiveGameplayEffectHandle DurationEffectHandle;
	float RemainingAmount = 0.f;
	float ExpireServerTime = 0.f;
	FObjectKey SourceKey;
};

/**
 * 서로 다른 스킬이 부여한 개인 실드를 독립적으로 유지한다.
 * Duration GE는 시간/태그/Cue를 담당하고, 실제 남은 실드량은 이 컴포넌트가 관리한다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRShieldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRShieldComponent();
	virtual void BeginPlay() override;

	/** 서버에서 Duration GE와 함께 독립 실드 레이어를 부여한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Shield")
	bool GrantShield(
		float Amount,
		float Duration,
		TSubclassOf<UGameplayEffect> DurationEffectClass);

	/** 기존 PersonalShield Duration GE와 동일 출처의 실드 부여를 하나의 레이어로 등록한다. */
	bool RegisterPersonalShieldGrant(float Amount, const UObject* SourceObject);

	/** 먼저 만료되는 레이어부터 피해를 흡수하고, 남은 피해량을 반환한다. */
	float AbsorbDamage(float IncomingDamage);

	/** 사망·리스폰·게임 초기화 시 모든 레이어를 제거한다. */
	void ClearShieldLayers();

	bool HasShieldLayers() const { return !ShieldLayers.IsEmpty(); }

private:
	UAbilitySystemComponent* GetAbilitySystemComponent() const;
	void HandleActiveGameplayEffectAdded(
		UAbilitySystemComponent* TargetAbilitySystem,
		const FGameplayEffectSpec& EffectSpec,
		FActiveGameplayEffectHandle EffectHandle);
	void HandleLayerEffectRemoved(FActiveGameplayEffectHandle EffectHandle);
	bool AddShieldLayer(
		float Amount,
		FActiveGameplayEffectHandle DurationEffectHandle,
		const UObject* SourceObject);
	void RemoveShieldLayer(FActiveGameplayEffectHandle EffectHandle, bool bRemoveDurationEffect);
	void RefreshTotalShield();

	/** 서버 전용: HUD는 합산 Shield Attribute 복제값을 사용한다. */
	TArray<FDRShieldLayer> ShieldLayers;
	TMap<FObjectKey, FActiveGameplayEffectHandle> LatestDurationHandlesBySource;
	bool bIsRemovingLayer = false;
};
