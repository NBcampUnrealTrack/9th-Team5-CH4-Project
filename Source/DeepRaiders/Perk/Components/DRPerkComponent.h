#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Item/GAS/DRItemAbilitySet.h"
#include "DRPerkComponent.generated.h"

class UDRPerkDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRPerksChangedSignature);

UCLASS(ClassGroup = (DeepRaiders))
class DEEPRAIDERS_API UDRPerkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 플레이어별 퍽 상태를 관리하는 복제 컴포넌트를 초기화한다. */
	UDRPerkComponent();

	/** 테스트용 퍽 배열을 소유 클라이언트에 복제하도록 등록한다. */
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 테스트 배열에 등록된 동일 퍽 개수를 반환한다. */
	int32 GetTestPerkCount(const UDRPerkDefinition* PerkDefinition) const;

	/** 테스트용 전체 퍽 슬롯 제한 안에서 퍽을 추가할 수 있는지 확인한다. */
	bool CanAddTestPerk(const UDRPerkDefinition* PerkDefinition) const;

	/** 서버에서 테스트 배열에 퍽을 추가하고 AbilitySet을 즉시 적용한다. */
	bool AddTestPerk(UDRPerkDefinition* PerkDefinition);

	/** 서버에서 모든 퍽 슬롯과 적용된 AbilitySet을 초기화한다. */
	bool ResetPerks();

	/** 소유 클라이언트에서 서버에 퍽 초기화를 요청한다. */
	void RequestResetPerks();

	UPROPERTY(BlueprintAssignable, Category = "Perk")
	FDRPerksChangedSignature OnPerksChanged;

private:
	UFUNCTION(Server, Reliable)
	void ServerResetPerks();

	/** 복제된 테스트용 퍽 배열의 변경을 소유 클라이언트에 알린다. */
	UFUNCTION()
	void OnRep_TestPerkSlots();

	/** 임시 테스트용 데이터이며 정식 퍽 보유 구조로 사용하지 않는다. */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_TestPerkSlots,
		Category = "Perk|Test",
		meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<UDRPerkDefinition>> TestPerkSlots;

	/** 서버에서 각 퍽이 ASC에 부여한 Ability와 Effect 핸들을 슬롯 순서대로 보관한다. */
	TArray<FDRItemAbilitySet_GrantedHandles> TestGrantedHandles;

	/** 테스트 배열에 넣을 수 있는 전체 퍽 개수다. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Perk|Test",
		meta = (AllowPrivateAccess = true, ClampMin = 1, UIMin = 1))
	int32 TestMaxPerkSlotCount = 5;

};
