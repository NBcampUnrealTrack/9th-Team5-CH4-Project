#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRZiplineEndpoint.generated.h"

class USceneComponent;

/**
 * Zipline Rope의 월드 Anchor.
 *
 * 이동 설정 / 상호작용 / Visual은 ADRZiplineRope가 소유하고,
 * Endpoint는 레벨에서 Rope의 시작/끝 위치만 제공한다.
 */
UCLASS()
class DEEPRAIDERS_API ADRZiplineEndpoint : public AActor
{
	GENERATED_BODY()

public:
	ADRZiplineEndpoint();

	UFUNCTION(BlueprintPure, Category = "Zipline")
	FVector GetAnchorLocation() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zipline", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zipline", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> RopeAnchor;
};
