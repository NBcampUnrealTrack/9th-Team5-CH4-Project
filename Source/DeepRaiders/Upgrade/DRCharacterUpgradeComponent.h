#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "DRUpgradeTypes.h"
#include "DRCharacterUpgradeComponent.generated.h"

class UDRCharacterUpgradeProfile;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRCharacterUpgradesChanged);

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRCharacterUpgradeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRCharacterUpgradeComponent();

	UFUNCTION(BlueprintPure, Category = "Upgrade")
	int32 GetUpgradeLevel(FGameplayTag UpgradeTag) const;

	const UDRCharacterUpgradeProfile* GetProfile() const;

	const FDRStatUpgradeData* GetUpgradeData(FGameplayTag UpgradeTag) const;
	bool TryUpgrade(FGameplayTag UpgradeTag, int32 ExpectedLevel);
	void ResetUpgrades();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Upgrade")
	FDRCharacterUpgradesChanged OnUpgradesChanged;

private:
	bool BuildEffectValues(UAbilitySystemComponent& AbilitySystem, FGameplayTag UpgradeTag,
		int32 NextLevel, TMap<FGameplayTag, float>& Values) const;
	bool ApplyUpgradeEffect(UAbilitySystemComponent& AbilitySystem, FGameplayTag UpgradeTag,
		const TMap<FGameplayTag, float>& Values);

	UFUNCTION()
	void OnRep_Upgrades();

	UPROPERTY(EditDefaultsOnly, Category = "Upgrade")
	TObjectPtr<UDRCharacterUpgradeProfile> Profile;

	UPROPERTY(ReplicatedUsing = OnRep_Upgrades)
	TArray<FDRUpgradeState> Upgrades;

	FActiveGameplayEffectHandle EffectHandle;
	bool IsUpdating = false;
};
