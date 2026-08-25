#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "DRWorldItemTypes.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRWorldItemSpawnParams
{
	GENERATED_BODY()
	
	/*
	 * 아이템이 시각적으로 튀어나오기 시작하는 위치
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Item")
	FTransform SourceTransform = FTransform::Identity;
	
	// 아이템이 최종적으로 부유할 대략적인 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Item")
	FTransform TargetTransform = FTransform::Identity;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Item")
	TObjectPtr<AActor> IgnoredActor = nullptr;
	
	// false면 궤적 없이 바로 상호작용 가능한 상태로 생성
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Item")
	bool bPlayEmergence = true;
};

USTRUCT()
struct DEEPRAIDERS_API FDRWorldItemEmergenceData
{
	GENERATED_BODY()
	
	UPROPERTY()
	FVector SourceWorldLocation = FVector::ZeroVector;
	
	UPROPERTY()
	float StartServerTime = -1.f;
};