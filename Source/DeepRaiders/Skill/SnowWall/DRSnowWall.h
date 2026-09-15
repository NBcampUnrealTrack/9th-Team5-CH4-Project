#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRSnowWall.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** 서버가 생성하고 복제하는 실제 눈 벽이다. 메시가 없어도 BoxCollision으로 길을 막는다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRSnowWall : public AActor
{
	GENERATED_BODY()

public:
	ADRSnowWall();

	/** 복셀 눈벽 생성이 끝날 때까지 사용할 임시 충돌 크기만 설정한다. */
	void ConfigureWall(const FVector& InDimensions);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow Wall")
	TObjectPtr<UBoxComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow Wall")
	TObjectPtr<UStaticMeshComponent> MeshComponent;
};
