#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRPlayerLifecycleComponent.generated.h"

class ADRPlayerCharacter;
class UCameraShakeBase;
class UAbilitySystemComponent;
struct FGameplayTag;

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
	
private:
	ADRPlayerCharacter* GetOwnerCharacter() const;

	// Fall Damage
	float CalculateFallDamage(float LandingSpeed) const;
	void ApplyFallDamage(float LandingSpeed);

	UFUNCTION(Client, Unreliable)
	void ClientPlayFallFeedback(bool bTookFallDamage, bool bDied);
	void PlayLocalCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.f);

	// Death
	void HandleDeathFromServer();
	void ApplyDeathRagdoll();
	void ClearDeathRagdollPresentation();

	void HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	FDelegateHandle DeadTagChangedHandle;
	
	// Respawn
	void RespawnAtRagdollLocation();
	bool TryFindRagdollRespawnTransform(FTransform& OutRespawnTransform) const;
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
