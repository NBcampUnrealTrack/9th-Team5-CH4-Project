#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DRPlayerState.generated.h"

class FLifetimeProperty;

UCLASS()
class DEEPRAIDERS_API ADRPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Player|Jetpack")
	bool HasJetpack() const
	{
		return bHasJetpack;
	}

	UFUNCTION(BlueprintPure, Category = "Player|Jetpack")
	float GetJetpackFuel() const
	{
		return CurrentJetpackFuel;
	}

	UFUNCTION(BlueprintPure, Category = "Player|Jetpack")
	float GetMaxJetpackFuel() const
	{
		return MaxJetpackFuel;
	}

	UFUNCTION(BlueprintPure, Category = "Player|Jetpack")
	float GetJetpackFuelRatio() const
	{
		if (MaxJetpackFuel <= 0.f)
		{
			return 0.f;
		}

		return FMath::Clamp(
			CurrentJetpackFuel / MaxJetpackFuel,
			0.f,
			1.f);
	}

	/** 서버에서 플레이어에게 제트팩을 지급한다. */
	void GrantJetpack();

	/** 서버에서 연료를 소비한다. */
	bool ConsumeJetpackFuel(float Amount);

protected:
	/** 모든 플레이어가 알아야 하는 제트팩 보유 상태 */
	UPROPERTY(
		ReplicatedUsing = OnRep_HasJetpack,
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Player|Jetpack")
	bool bHasJetpack = false;

	/** 소유 플레이어 UI에서 사용할 현재 연료 */
	UPROPERTY(
		ReplicatedUsing = OnRep_JetpackFuel,
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Player|Jetpack")
	float CurrentJetpackFuel = 0.f;

	/** 프로토타입에서는 모든 인스턴스가 같은 기본값을 사용한다. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Player|Jetpack")
	float MaxJetpackFuel = 100.f;

	UFUNCTION()
	void OnRep_HasJetpack();

	UFUNCTION()
	void OnRep_JetpackFuel();

private:
	/** 연결된 Pawn의 제트팩 외형을 현재 상태에 맞게 갱신한다. */
	void RefreshJetpackVisualOnPawn();
};