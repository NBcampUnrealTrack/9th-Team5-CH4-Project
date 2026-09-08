#pragma once

#include "CoreMinimal.h"
#include "DRBarrierGenerator.h"
#include "DRForwardBarrierGenerator.generated.h"

class UStaticMeshComponent;

/** BP에서 지정한 반구 Static Mesh를 사용하는 전방 방벽 생성기다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRForwardBarrierGenerator : public ADRBarrierGenerator
{
	GENERATED_BODY()

public:
	ADRForwardBarrierGenerator();
	virtual UPrimitiveComponent* GetBarrierCollisionComponent() const override;

protected:
	virtual void BeginPlay() override;
	virtual void RefreshBarrierGeometry() override;

	/** 외형과 물리 투사체 Overlap 판정을 함께 담당한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier|Forward")
	TObjectPtr<UStaticMeshComponent> ForwardBarrierMesh;

	/** ForwardBarrierMesh와 같은 메시를 사용하며 히트스캔 Trace만 막는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier|Forward")
	TObjectPtr<UStaticMeshComponent> ForwardBarrierTraceCollision;
};
