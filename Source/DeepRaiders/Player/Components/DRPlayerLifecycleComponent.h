#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRPlayerLifecycleComponent.generated.h"

class ADRPlayerCharacter;
class USoundBase;
class UCameraShakeBase;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;
class UGameplayEffect;

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRPlayerLifecycleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRPlayerLifecycleComponent();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
public:
	/**
	 * Character::Landed에서 호출한다.
	 *
	 * 서버에서 Fall Damage / Fall Feedback /
	 * 착지 후 Jetpack Fuel 충전을 처리한다.
	 */
	void HandleLanded(float LandingSpeed);

	/**
	 * PossessedBy / OnRep_Controller에서 호출한다.
	 * Respawn된 새 Pawn의 입력 상태를 정상화한다.
	 */
	void HandleControllerReady();

	void BindAbilitySystem(UAbilitySystemComponent* ASC);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Death")
	TSubclassOf<UGameplayEffect> DeadEffectClass;
	
private:
	ADRPlayerCharacter* GetOwnerCharacter() const;

	// ==============================
	// Fall Damage
	// ==============================

	float CalculateFallDamage(float LandingSpeed) const;

	void ApplyFallDamage(float LandingSpeed);

	UFUNCTION(Client, Unreliable)
	void ClientPlayFallFeedback(bool bTookFallDamage, bool bDied);

	void PlayLocalCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.f);

	// ==============================
	// Death
	// ==============================

	void HandleHealthDepleted();

	/** 서버에서 실제 사망 절차를 시작한다. */
	void HandleDeathFromServer();

	/** 각 인스턴스에서 Ragdoll 표현을 적용한다. */
	void ApplyDeathRagdoll();
	
	// ==============================
	// Respawn
	// ==============================

	void RespawnAtRagdollLocation();

	bool TryFindRagdollRespawnTransform(FTransform& OutRespawnTransform) const;
	
	bool bDeathRagdollApplied = false;

	FTimerHandle RespawnTimerHandle;

	void UnbindAbilitySystem();

	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	FDelegateHandle HealthChangedDelegateHandle;
	
protected:
	// ==============================
	// Respawn Settings
	// ==============================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Respawn")
	FName RespawnRagdollBoneName = TEXT("pelvis");

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

	// ==============================
	// Fall Damage Settings
	// ==============================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Fall Damage", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinFallDamageSpeed = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Fall Damage", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxFallDamageSpeed = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Fall Damage", meta = ( ClampMin = "0.0", ClampMax = "1.0"))
	float MaxFallDamageRatio = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Fall Damage", meta = (ClampMin = "0.01"))
	float FallDamageExponent = 2.f;

	// ==============================
	// Fall Presentation
	// ==============================

	UPROPERTY(EditDefaultsOnly, Category = "Lifecycle|Fall|Sound")
	TObjectPtr<USoundBase> FallSound;

	UPROPERTY(EditDefaultsOnly, Category = "Lifecycle|Fall|Sound")
	TObjectPtr<USoundBase> FallDamageSound;

	UPROPERTY(EditDefaultsOnly, Category = "Lifecycle|Fall|Sound")
	TObjectPtr<USoundBase> FallDeadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifecycle|Fall|Camera Shake")
	TSubclassOf<UCameraShakeBase> FallDamageCameraShakeClass;
};
