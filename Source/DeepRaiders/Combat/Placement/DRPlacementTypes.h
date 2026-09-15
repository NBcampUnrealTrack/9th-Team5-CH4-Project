#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DRPlacementTypes.generated.h"

/** 설치형 스킬이 공통으로 사용하는 조준 및 설치 가능 여부 판정 설정이다. */
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRPlacementSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Targeting", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxDistance = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Targeting")
	TEnumAsByte<ECollisionChannel> AimTraceChannel = ECC_Visibility;

	/** 바닥으로 인정할 최대 경사. 벽/천장은 대상이 될 수 없도록 45도를 상한으로 둔다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
	float MaxFloorSlopeDegrees = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0", Units = "cm"))
	float ServerViewOriginTolerance = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Validation", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float ServerAimAngleTolerance = 5.f;

	/** 마우스 휠 입력 한 칸에 적용할 프리뷰 회전 각도다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Rotation", meta = (ClampMin = "1.0", ClampMax = "180.0", Units = "deg"))
	float RotationStepDegrees = 15.f;
};

/** 설치 지점과 함께 클라이언트가 선택한 회전 오프셋을 서버로 전달한다. */
USTRUCT()
struct DEEPRAIDERS_API FDRGameplayAbilityTargetData_Placement
	: public FGameplayAbilityTargetData_SingleTargetHit
{
	GENERATED_BODY()

	FDRGameplayAbilityTargetData_Placement() = default;

	FDRGameplayAbilityTargetData_Placement(const FHitResult& InHitResult, float InRotationOffsetDegrees)
		: FGameplayAbilityTargetData_SingleTargetHit(InHitResult)
		, RotationOffsetDegrees(InRotationOffsetDegrees)
	{
	}

	UPROPERTY()
	float RotationOffsetDegrees = 0.f;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		const bool bBaseSuccess =
			FGameplayAbilityTargetData_SingleTargetHit::NetSerialize(Ar, Map, bOutSuccess);
		Ar << RotationOffsetDegrees;
		bOutSuccess = bOutSuccess && bBaseSuccess && !Ar.IsError();
		return bOutSuccess;
	}
};

template<>
struct TStructOpsTypeTraits<FDRGameplayAbilityTargetData_Placement>
	: public TStructOpsTypeTraitsBase2<FDRGameplayAbilityTargetData_Placement>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
