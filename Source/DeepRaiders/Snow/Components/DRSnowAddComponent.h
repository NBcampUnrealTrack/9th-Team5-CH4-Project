#pragma once

#include "CoreMinimal.h"
#include "DRSnowInteractionComponent.h"
#include "DRSnowAddComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDRSnowAddedSignature,
	const FDRSnowSurfaceAddRequest&,
	Request,
	bool,
	bHandled);

// 투사체, 폭발, 스킬 장판처럼 표면에 눈을 쌓는 Actor에 붙인다.
UCLASS(
	ClassGroup = (Snow),
	BlueprintType,
	Blueprintable,
	meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRSnowAddComponent : public UDRSnowInteractionComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	bool TryAddSnowFromHit(const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = "Snow|Add")
	bool TryAddSnowAtLocation(
		FVector WorldLocation,
		FVector SurfaceNormal);

protected:
	FDRSnowSurfaceAddRequest MakeAddRequest(
		FVector WorldLocation,
		FVector SurfaceNormal) const;

public:
	UPROPERTY(BlueprintAssignable, Category = "Snow|Add")
	FDRSnowAddedSignature OnSnowAdded;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Add", meta = (ClampMin = "0.0", Units = "cm"))
	float AddRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Add", meta = (ClampMin = "0.0"))
	float AddAmount = 1.f;
};
