#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRHealthComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(
	FDROnHealthDepleted);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FDROnHealthChanged,
	float, // OldHealth
	float  // NewHealth
);

UCLASS(
	ClassGroup = (Player),
	meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRHealthComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UDRHealthComponent();

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps)
		const override;

	/**
	 * 서버에서 체력을 감소시킨다.
	 * 실제 적용된 Damage를 반환한다.
	 */
	float ApplyDamage(float DamageAmount);

	float GetCurrentHealth() const
	{
		return CurrentHealth;
	}

	float GetMaxHealth() const
	{
		return MaxHealth;
	}

	float GetHealthRatio() const
	{
		if (MaxHealth <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}

		return FMath::Clamp(
			CurrentHealth / MaxHealth,
			0.f,
			1.f);
	}

	bool IsDead() const
	{
		return CurrentHealth <=
			KINDA_SMALL_NUMBER;
	}

	FDROnHealthDepleted OnHealthDepleted;
	FDROnHealthChanged OnHealthChanged;

private:
	UFUNCTION()
	void OnRep_CurrentHealth();

protected:
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Health",
		meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(
		ReplicatedUsing = OnRep_CurrentHealth,
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Health")
	float CurrentHealth = 100.f;

private:
	/**
	 * 클라이언트 RepNotify에서
	 * 이전 Health를 알 수 있도록 보관하는 표시값.
	 */
	float LastObservedHealth = 100.f;
};