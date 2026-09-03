#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "DRThrowActionTypes.generated.h"

class AActor;
class UNiagaraSystem;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRThrowActionSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Spawn")
	FName ThrowSocketName = TEXT("ThrowPoint");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Spawn")
	float FallbackForwardOffset = 50.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Spawn")
	float FallbackRightOffset = 20.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Spawn")
	float FallbackHeightOffset = 50.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Collision")
	TEnumAsByte<ECollisionChannel> AimTraceChannel = ECC_GameTraceChannel1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Collsion")
	TEnumAsByte<ECollisionChannel> ExplosionOcclusionTraceChannel = ECC_Visibility;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Preview", meta = (ClampMin = "0.0", Units = "cm"))
	float PreviewProjectileRadius = 12.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Preview", meta = (ClampMin = "0.1", Units = "s"))
	float MaxSimulationTime = 2.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Preview", meta = (ClampMin = "5.0", ClampMax = "60.0"))
	float SimulationFrequency = 20.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Preview")
	TObjectPtr<UNiagaraSystem> TrajectoryPreviewSystem;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Preview")
	FName TrajectoryPointsParameter = TEXT("User.TrajectoryPoints");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Preview")
	FName TrajectoryDirectionsParameter = TEXT("User.TrajectoryDirections");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Preview")
	FName SpherePointParameter = TEXT("User.SpherePoint");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Preview")
	FName SphereScaleParameter = TEXT("User.SphereScale");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Validation", meta = (ClampMin = "0.0", Units = "cm"))
	float ServerViewOriginTolerance = 150.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw|Validation",
		meta = (ClamapMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float ServerAimAngleTolerance = 20.f;	
};

namespace DRThrow
{
	// 던져지는 위치 반환, Socket이 유효하다면 Socket 위치 없다면 오프셋 기반으로 반환
	DEEPRAIDERS_API FVector ResolveLaunchLocation(const AActor* AvatarActor, const FDRThrowActionSettings& Settings,
		const FVector& AimDirection);
}










