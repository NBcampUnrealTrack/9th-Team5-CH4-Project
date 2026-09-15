#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRPlacementPreviewActor.generated.h"

/**
 * 설치형 스킬의 시각적 프리뷰 전용 Actor다.
 * 파생 Blueprint가 메시, Niagara, 머티리얼과 유효/무효 연출을 결정한다.
 */
UCLASS(Abstract, Blueprintable, NotPlaceable)
class DEEPRAIDERS_API ADRPlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ADRPlacementPreviewActor();

	void UpdatePreview(const FVector& Location, const FVector& SurfaceNormal,
		const FVector& AimDirection, bool bCanPlace, const FVector& Dimensions);
	void HidePreview();

	UFUNCTION(BlueprintImplementableEvent, Category = "Placement Preview")
	void OnPreviewUpdated(FVector SurfaceNormal, FVector AimDirection, bool bCanPlace,
		FVector Dimensions);
};
