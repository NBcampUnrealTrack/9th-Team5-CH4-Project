#pragma once

#include "CoreMinimal.h"
#include "DRGrappleAbilityTypes.generated.h"

class UNiagaraSystem;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRGrappleAbilitySettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "1.0", Units = "cm"))
	float MaxDistance = 1200.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float PullAcceleration = 2600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxSpeed = 2400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "0.0"))
	float ControlScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement",
		meta = (ClampMin = "0.0", Units = "cm"))
	float ArrivalDistance = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Validation",
		meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float ServerAimAngleTolerance = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Validation",
		meta = (ClampMin = "0.0", Units = "cm"))
	float ServerViewOriginTolerance = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Trace")
	TEnumAsByte<ECollisionChannel> AimTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Presentation")
	TObjectPtr<UNiagaraSystem> AimMarkerSystem;	
};