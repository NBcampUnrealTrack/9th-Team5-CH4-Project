#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRPlayerLifecycleComponent.generated.h"

class ADRPlayerCharacter;
class UCameraShakeBase;
class UAbilitySystemComponent;
class UGameplayEffect;
class UDRItemDefinition;
class ADRPlayerState;
struct FDRItemInstance;
struct FGameplayTag;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRDeathCurrencyDropEntry
{
	GENERATED_BODY()

	/** 사망 시 월드에 생성할 금전 아이템. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency Drop")
	TObjectPtr<UDRItemDefinition> ItemDefinition = nullptr;

	/** 이 아이템 하나가 사망 손실 계산에서 나타내는 SnowGauge 값. 실제 PickupEffect 값과 달라도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency Drop", meta = (ClampMin = "1", UIMin = "1"))
	int32 SnowGaugeValue = 1;
};

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRPlayerLifecycleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRPlayerLifecycleComponent();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
public:
	void HandleLanded(float LandingSpeed);
	void HandleControllerReady();

	void BindAbilitySystem(UAbilitySystemComponent* ASC);
	void ApplyRagdollKnockback(const FVector& Origin, float Distance);
	
private:
	ADRPlayerCharacter* GetOwnerCharacter() const;

	// Fall Damage
	float CalculateFallDamage(float LandingSpeed) const;
	void ApplyFallDamage(float LandingSpeed);

	void PlayLocalCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.f);

	// Death
	void HandleDeathFromServer();
	void DropDeathItemsFromServer();
	void AppendCurrencyDeathDropsFromServer(ADRPlayerState* PlayerState,
		TArray<FDRItemInstance>& OutDroppedItems) const;
	void ApplyDeathRagdoll();
	void ClearDeathRagdollPresentation();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplyRagdollKnockback(FVector_NetQuantizeNormal Direction, float VelocityChange);

	void HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	FDelegateHandle DeadTagChangedHandle;
	
	// Respawn
	void RespawnAtPlayerStart();
	void RespawnAtRagdollLocation();
	bool TryFindRagdollRespawnTransform(FTransform& OutRespawnTransform) const;
	void ApplyRespawnInvincibility(ADRPlayerCharacter* RespawnedCharacter) const;
	bool bDeathRagdollApplied = false;
	FTimerHandle RespawnTimerHandle;
	
	void UnbindAbilitySystem();
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	
	// Sound
	void ExecuteFallSoundCueFromServer(bool bTookFallDamage, bool bDied);
	
protected:
	// Respawn Settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnDelay = 3.f;

	/** 사망 후 리스폰한 Pawn에 적용할 시간제 무적 Gameplay Effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn")
	TSubclassOf<UGameplayEffect> RespawnInvincibilityEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn")
	FName RespawnRagdollBoneName = TEXT("pelvis");

	/** 넉백 거리(cm)를 래그돌 전체에 적용할 속도 변화(cm/s)로 변환한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Death", meta = (ClampMin = "0.0"))
	float RagdollKnockbackVelocityScale = 4.f;

	/** 사망 시 사용할 금전 아이템과 손실 계산용 가치. 런타임에서는 앞의 유효한 항목 두 개만 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Death|Currency Drop",
		meta = (TitleProperty = "ItemDefinition"))
	TArray<FDRDeathCurrencyDropEntry> CurrencyDropEntries;

	/** 현재 SnowGauge 중 사망 시 손실 후보로 계산할 비율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Death|Currency Drop",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SnowGaugeLossRatio = 0.f;

	/** SnowGauge 손실과 무관하게 항상 추가로 생성하는 고정 처치 보상 개수. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Death|Currency Drop", meta = (ClampMin = "0"))
	int32 MinimumCurrencyDropCount = 0;

	/** SnowGauge 손실을 나타내는 금전 아이템의 최대 개수. 고정 처치 보상은 이 제한에서 제외한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Death|Currency Drop", meta = (ClampMin = "0"))
	int32 MaximumLossCurrencyDropCount = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn", meta = (ClampMin = "0.0", Units = "cm"))
	float RespawnGroundTraceDistance = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn", meta = (ClampMin = "0.0", Units = "cm"))
	float RespawnGroundClearance = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn", meta = (ClampMin = "0.0", Units = "cm"))
	float RespawnSweepStartHeight = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn", meta = (ClampMin = "1.0", Units = "cm"))
	float RespawnSearchStep = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn", meta = (ClampMin = "0", ClampMax = "10"))
	int32 RespawnSearchRingCount = 3;

	// Fall Damage Settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Fall Damage", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinFallDamageSpeed = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Fall Damage", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxFallDamageSpeed = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Fall Damage", meta = ( ClampMin = "0.0", ClampMax = "1.0"))
	float MaxFallDamageRatio = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Fall Damage", meta = (ClampMin = "0.01"))
	float FallDamageExponent = 2.f;
};
