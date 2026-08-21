#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/GAS/DRAbilitySet.h"
#include "DRPerkComponent.generated.h"

class UDRPerkDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRPerksChangedSignature);

/** 플레이어가 보유한 퍽 하나의 정의와 서버 GAS 적용 상태다. */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRPerkEntry
{
	GENERATED_BODY()

	/** 소유 클라이언트에 복제할 퍽 정의다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk")
	TObjectPtr<UDRPerkDefinition> PerkDefinition;

	/** 초기화 시 Ability와 Effect를 회수하기 위한 서버 전용 핸들이다. */
	FDRAbilitySet_GrantedHandles GrantedHandles;
};

UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRPerkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 플레이어별 퍽 상태를 관리하는 복제 컴포넌트를 초기화한다. */
	UDRPerkComponent();

	/** 보유 퍽 목록을 소유 클라이언트에 복제하도록 등록한다. */
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 보유 중인 동일 퍽 개수를 반환한다. */
	int32 GetPerkCount(const UDRPerkDefinition* PerkDefinition) const;

	/** UI에 표시할 현재 퍽 목록을 반환한다. */
	const TArray<FDRPerkEntry>& GetPerkEntries() const
	{
		return PerkEntries;
	}

	/** UI에 표시할 전체 퍽 슬롯 수를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Perk")
	int32 GetMaxPerkSlotCount() const
	{
		return MaxPerkSlotCount;
	}

	/** 전체 퍽 슬롯 제한 안에서 퍽을 추가할 수 있는지 확인한다. */
	bool CanAddPerk(const UDRPerkDefinition* PerkDefinition) const;

	/** 서버에서 퍽을 추가하고 AbilitySet을 즉시 적용한다. */
	bool AddPerk(UDRPerkDefinition* PerkDefinition);

	/** 서버에서 모든 퍽 슬롯과 적용된 AbilitySet을 초기화한다. */
	bool ResetPerks();

	/** 소유 클라이언트에서 서버에 퍽 초기화를 요청한다. */
	void RequestResetPerks();

	UPROPERTY(BlueprintAssignable, Category = "Perk")
	FDRPerksChangedSignature OnPerksChanged;

private:
	/** 소유 클라이언트의 초기화 요청을 서버에서 실행한다. */
	UFUNCTION(Server, Reliable)
	void ServerResetPerks();

	/** 복제된 퍽 목록의 변경을 소유 클라이언트에 알린다. */
	UFUNCTION()
	void OnRep_PerkEntries();

	/** 플레이어가 보유한 퍽과 서버에서 적용한 AbilitySet 핸들을 관리한다. */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_PerkEntries,
		Category = "Perk",
		meta = (AllowPrivateAccess = true))
	TArray<FDRPerkEntry> PerkEntries;

	/** 보유할 수 있는 전체 퍽 개수다. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Perk",
		meta = (AllowPrivateAccess = true, ClampMin = 1, UIMin = 1))
	int32 MaxPerkSlotCount = 5;

};
