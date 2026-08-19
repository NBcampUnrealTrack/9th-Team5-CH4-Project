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
	/** 플레이어별 퍽 상태를 관리하는 복제 컴포넌트를 초기화한다. */
	UDRPerkComponent();

	/** 플레이어의 퍽 랭크를 소유 클라이언트에 복제하도록 등록한다. */
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** RowName에 해당하는 현재 플레이어의 퍽 랭크를 반환한다. */
	int32 GetPerkRank(FName RowName) const;

	/** 다음 랭크와 GameplayEffect가 적용 가능한 상태인지 확인한다. */
	bool CanApplyNextRank(FName RowName, const FDRPerkTableRow& PerkRow) const;

	/** 서버에서 다음 랭크 GameplayEffect를 적용하고 플레이어 상태를 갱신한다. */
	bool ApplyNextRank(FName RowName, const FDRPerkTableRow& PerkRow);

	UPROPERTY(BlueprintAssignable, Category = "Perk")
	FDRPerksChangedSignature OnPerksChanged;

private:
	/** 복제된 퍽 상태를 받은 소유 클라이언트에 변경을 알린다. */
	UFUNCTION()
	void OnRep_PerkStates();

	/** RowName에 해당하는 변경 가능한 플레이어 퍽 상태를 찾는다. */
	FDRPerkState* FindPerkState(FName RowName);

	UPROPERTY(ReplicatedUsing = OnRep_PerkStates)
	TArray<FDRPerkState> PerkStates;

	TMap<FName, FActiveGameplayEffectHandle> EffectHandles;
};
