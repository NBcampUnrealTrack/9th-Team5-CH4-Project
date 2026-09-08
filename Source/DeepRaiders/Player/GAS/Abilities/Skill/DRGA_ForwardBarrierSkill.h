#pragma once

#include "CoreMinimal.h"
#include "DRGA_BarrierSkill.h"
#include "DRGA_ForwardBarrierSkill.generated.h"

/** 사용자 전방에 기존 배리어 생성기를 설치하는 창던지기 교체 스킬이다. */
UCLASS()
class DEEPRAIDERS_API UDRGA_ForwardBarrierSkill : public UDRGA_BarrierSkill
{
	GENERATED_BODY()

public:
	UDRGA_ForwardBarrierSkill();

protected:
	virtual bool ResolveBarrierSpawnTransform(
		ADRPlayerCharacter* Character,
		FTransform& OutSpawnTransform) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Barrier", meta = (ClampMin = "0.0", Units = "cm"))
	float ForwardDistance = 500.f;
};
