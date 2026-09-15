#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DRGrappleAbilityTypes.generated.h"

class UNiagaraSystem;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRGrappleAbilitySettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "1.0", Units = "cm"))
	float MaxDistance = 1200.f;
	
	// 히트스캔 성공 후 훅 연출이 목표 위치까지 이동하는 속도다.
	// GA가 이 값으로 실제 이동 시작 시점을 결정하고 GameplayCue에도 같은 시간을 전달한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Presentation",
		meta = (ClampMin = "1.0", Units = "cm/s"))
	float HookTravelSpeed = 6000.f;

	// 가까운 목표에서도 부착 연출을 확인할 수 있도록 보장하는 최소 비행 시간이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Presentation",
		meta = (ClampMin = "0.0", Units = "s"))
	float MinimumHookFlightDuration = 0.05f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float PullAcceleration = 2600.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxSpeed = 2400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "0.0"))
	float ControlScale = 1.0f;
	
	// 0이면 훅을 직접 향하고, 1이면 훅 방향과 시선 방향을 같은 비율로 혼합한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement", meta = (ClampMin = "0.0"))
	float ViewPullWeight = 1.f;
	
	// GuaranteeClosingSpeed에서 매 순간 보장할 훅 방향의 최소 속도다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinimumClosingSpeed = 400.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "0.0", Units = "cm"))
	float ArrivalDistance = 100.f;

	// 그래플 시작 직후 시선 뒤쪽 판정을 무시할 시간이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Duration", meta = (ClampMin = "0.0", Units = "s"))
	float ViewDetachProtectionDuration = 1.f;

	// 0보다 크면 해당 시간이 지난 후 그래플을 자동 종료한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Duration", meta = (ClampMin = "0.0", Units = "s"))
	float MaxDuration = 4.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Validation",
		meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float ServerAimAngleTolerance = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Validation",
		meta = (ClampMin = "0.0", Units = "cm"))
	float ServerViewOriginTolerance = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Trace")
	TEnumAsByte<ECollisionChannel> AimTraceChannel = DRCollisionChannels::Grapple;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Presentation")
	TObjectPtr<UNiagaraSystem> AimMarkerSystem;	
};