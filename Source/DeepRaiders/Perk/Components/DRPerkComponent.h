#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "DeepRaiders/Perk/DRPerkTable.h"
#include "DRPerkComponent.generated.h"

USTRUCT(BlueprintType)
struct FDRPerkState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk")
	FName RowName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk")
	int32 Rank = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRPerksChangedSignature);

UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRPerkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRPerkComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetPerkRank(FName RowName) const;
	bool CanApplyNextRank(FName RowName, const FDRPerkTableRow& PerkRow) const;
	bool ApplyNextRank(FName RowName, const FDRPerkTableRow& PerkRow);

	UPROPERTY(BlueprintAssignable, Category = "Perk")
	FDRPerksChangedSignature OnPerksChanged;

private:
	UFUNCTION()
	void OnRep_PerkStates();

	FDRPerkState* FindPerkState(FName RowName);

	UPROPERTY(ReplicatedUsing = OnRep_PerkStates)
	TArray<FDRPerkState> PerkStates;

	TMap<FName, FActiveGameplayEffectHandle> EffectHandles;
};
