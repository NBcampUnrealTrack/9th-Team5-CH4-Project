#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Item/DRItemActionTypes.h"
#include "DRItemActionPresentationComponent.generated.h"

class ADRPlayerCharacter;
class UAnimMontage;
class USoundBase;
class UCameraShakeBase;

UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRItemActionPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRItemActionPresentationComponent();

	/**
	 * 서버에서 확정된 Action의
	 * 3인칭 월드 연출을 전체 인스턴스에 전달한다.
	 */
	void PlayWorldActionFromServer(EDRItemActionType ActionType);

	/**
	 * 서버에서 Melee Hit가 확정됐을 때
	 * 공격자 / 피격자 / 월드 Impact 연출을 실행한다.
	 */
	void PlayMeleeHitFeedbackFromServer(ADRPlayerCharacter* HitPlayer, bool bKilled, const FVector& ImpactLocation);

	/**
	 * 기존 Character의 범용 Damage RPC 호환용.
	 * 현재 인스턴스의 로컬 피격 Shake를 재생한다.
	 */
	void PlayDamagedFeedbackLocal();

	/** 소유 클라이언트의 예측 사격 연출 */
	void PlayWeaponFireLocal(UAnimMontage* FireMontage);

	/** 서버에서 확정한 사격 연출을 다른 인스턴스에 전파 */
	void PlayWeaponFireFromServer(UAnimMontage* FireMontage);

private:
	ADRPlayerCharacter*
	GetOwnerCharacter() const;

	/** 현재 인스턴스에서 월드 몽타주를 재생한다. */
	void PlayWorldAction(EDRItemActionType ActionType);

	/** Action에 대응하는 월드 몽타주를 반환한다. */
	UAnimMontage* ResolveWorldActionMontage(EDRItemActionType ActionType) const;

	/** Action에 대응하는 사용 사운드를 반환한다. */
	USoundBase* ResolveActionSound(EDRItemActionType ActionType) const;

	/** 서버에서 확정한 월드 Action 연출을 전파한다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayWorldAction(EDRItemActionType ActionType);

	void PlayLocalCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.f);

	UFUNCTION(Client, Unreliable)
	void ClientPlayMeleeHitFeedback(bool bKilled);

	UFUNCTION(Client, Unreliable)
	void ClientPlayMeleeDamagedFeedback(bool bKilled);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMeleeImpactSound(bool bKilled, FVector_NetQuantize ImpactLocation);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayWeaponFire(UAnimMontage* FireMontage);

protected:
	// ==============================
	// World Presentation
	// ==============================

	/** 다른 플레이어에게 보이는 채굴 몽타주 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Action|World")
	TObjectPtr<UAnimMontage> WorldDigMontage;

	/** 다른 플레이어에게 보이는 근접 공격 몽타주 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Action|World")
	TObjectPtr<UAnimMontage> WorldMeleeAttackMontage;


	// ==============================
	// Sound
	// ==============================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Action|Sound")
	TObjectPtr<USoundBase> DigSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Action|Sound")
	TObjectPtr<USoundBase> MeleeAirSound;

	// ==============================
	// Melee Hit Feedback
	// ==============================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Action|Melee|Sound")
	TObjectPtr<USoundBase> MeleeHitSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Action|Melee|Sound")
	TObjectPtr<USoundBase> MeleeKillSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Action|Melee|Camera Shake")
	TSubclassOf<UCameraShakeBase> MeleeHitConfirmCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Action|Melee|Camera Shake")
	TSubclassOf<UCameraShakeBase> MeleeDamagedCameraShakeClass;
};
