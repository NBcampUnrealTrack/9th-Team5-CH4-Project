#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "TimerManager.h"
#include "DeepRaiders/GAS/DRAbilitySet.h"
#include "DRPlayerState.generated.h"

class FLifetimeProperty;
class UAbilitySystemComponent;
class UDRPlayerAttributeSet;
class UDRPerkComponent;
class UGameplayAbility;
class UGameplayEffect;
class UDRQuickSlotComponent;
class UDRItemDefinition;
struct FOnAttributeChangeData;

USTRUCT(BlueprintType)
struct FDRPublicQuickSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UDRItemDefinition> ItemDefinition;

	UPROPERTY(BlueprintReadOnly)
	int32 Quantity = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRPublicQuickSlotsChanged);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRCoinsChangedSignature,
	int32,
	NewCoins);

UCLASS()
class DEEPRAIDERS_API ADRPlayerState 
	: public APlayerState
	, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADRPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	const UDRPlayerAttributeSet* GetPlayerAttributeSet() const
	{
		return PlayerAttributeSet;
	}

	UDRPerkComponent* GetPerkComponent() const
	{
		return PerkComponent;
	}

	/** 서버 퀵슬롯을 팀 UI용 읽기 전용 스냅샷으로 갱신한다. */
	void UpdatePublicQuickSlots(const UDRQuickSlotComponent* QuickSlotComponent);

	const TArray<FDRPublicQuickSlot>& GetPublicQuickSlots() const
	{
		return PublicQuickSlots;
	}

	UPROPERTY(BlueprintAssignable, Category = "Player|Quick Slot")
	FDRPublicQuickSlotsChanged OnPublicQuickSlotsChanged;
	
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버에서 더 깊은 채굴 위치만 갱신
	bool UpdateDeepestDigLocation(const FVector& Location);

	UFUNCTION(BlueprintPure, Category = "Player|Mining")
	bool HasDeepestDigLocation() const { return bHasDeepestDigLocation; }

	UFUNCTION(BlueprintPure, Category = "Player|Mining")
	FVector GetDeepestDigLocation() const { return DeepestDigLocation; }

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
	
	/** 서버에서 제트팩 연료를 최대치까지 충전한다. */
	bool RefillJetpackFuel();

	UFUNCTION(BlueprintPure, Category = "Player|Coin")
	int32 GetCoins() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Coin")
	void SetCoins(int32 NewCoins);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Coin")
	void AddCoins(int32 Amount);

	UPROPERTY(BlueprintAssignable, Category = "Player|Coin")
	FDRCoinsChangedSignature OnCoinsChanged;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GAS|Lifecycle")
	void ResetForRespawn();
	
	UFUNCTION(BlueprintPure, Category = "GAS|Status")
	bool IsFrozen() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GAS|Status")
	void ClearFrozenState();
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Status")
	TSubclassOf<UGameplayEffect> FrozenEffectClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Status")
	TSubclassOf<UGameplayEffect> DeadEffectClass;
	
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void EvaluateDeadState();
	FDelegateHandle HealthChangedHandle;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
	TObjectPtr<UDRAbilitySet> DefaultAbilitySet;

	void GrantDefaultAbilities();
	
	FDRAbilitySet_GrantedHandles GrantedHandles;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UDRPlayerAttributeSet> PlayerAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Perk")
	TObjectPtr<UDRPerkComponent> PerkComponent;

	void BindStatusPolicy();
	void UnbindStatusPolicy();

	void HandleFreezeGaugeChanged(const FOnAttributeChangeData& Data);
	void HandleMaxFreezeGaugeChanged(const FOnAttributeChangeData& Data);
	
	void EvaluateFrozenState();

	// Freeze Decay
	void RestartFreezeDecay();
	void TickFreezeDecay();
	void StopFreezeDecay();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Status|Freeze", meta = (ClampMin = "0.0", Units = "s"))
	float FreezeDecayDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Status|Freeze", meta = (ClampMin = "0.01", Units = "s"))
	float FreezeDecayInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Status|Freeze", meta = (ClampMin = "0.0"))
	float FreezeDecayRatePerSecond = 10.f;

	FTimerHandle FreezeDecayTimerHandle;
	FDelegateHandle FreezeGaugeChangedHandle;
	FDelegateHandle MaxFreezeGaugeChangedHandle;
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Player|Mining")
	bool bHasDeepestDigLocation = false;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Player|Mining")
	FVector_NetQuantize DeepestDigLocation = FVector::ZeroVector;

	/** 모든 플레이어가 알아야 하는 제트팩 보유 상태 */
	UPROPERTY(ReplicatedUsing = OnRep_HasJetpack, VisibleAnywhere, BlueprintReadOnly, Category = "Player|Jetpack")
	bool bHasJetpack = false;

	/** 소유 플레이어 UI에서 사용할 현재 연료 */
	UPROPERTY(ReplicatedUsing = OnRep_JetpackFuel, VisibleAnywhere, BlueprintReadOnly, Category = "Player|Jetpack")
	float CurrentJetpackFuel = 0.f;

	/** 프로토타입에서는 모든 인스턴스가 같은 기본값을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Jetpack")
	float MaxJetpackFuel = 100.f;

	UFUNCTION()
	void OnRep_HasJetpack();

	UFUNCTION()
	void OnRep_JetpackFuel();

	UFUNCTION()
	void OnRep_Coins(int32 PreviousCoins);

	UPROPERTY(EditDefaultsOnly, ReplicatedUsing = OnRep_Coins, Category = "Player|Coin", meta = (ClampMin = "0"))
	int32 Coins = 1000;

private:
	UFUNCTION()
	void OnRep_PublicQuickSlots();

	UPROPERTY(ReplicatedUsing = OnRep_PublicQuickSlots)
	TArray<FDRPublicQuickSlot> PublicQuickSlots;

	/** 연결된 Pawn의 제트팩 외형을 현재 상태에 맞게 갱신한다. */
	void RefreshJetpackVisualOnPawn();

#pragma region Teleport
public:
	UFUNCTION(BlueprintPure, Category = "Player|Teleport")
	int32 GetTeamId() const { return TeamId != INDEX_NONE ? TeamId : GetPlayerId(); }

	void SetTeamId(int32 NewTeamId);

private:
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Player|Teleport", meta = (AllowPrivateAccess = "true"))
	int32 TeamId = INDEX_NONE;
#pragma endregion
};
