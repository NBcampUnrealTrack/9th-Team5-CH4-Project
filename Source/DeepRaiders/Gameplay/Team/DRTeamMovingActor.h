#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTeamMovingActor.generated.h"

class USceneComponent;

/** 자신의 팀 활성 상태에 따라 원위치와 오프셋 위치 사이를 이동한다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRTeamMovingActor : public AActor
{
	GENERATED_BODY()

public:
	ADRTeamMovingActor();

	void SetTeamActive(bool bActive, bool bImmediate = false);
	void RefreshTeamColor();

	int32 GetTeamId() const
	{
		return TeamId;
	}

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Team Movement")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 내부 TeamId 0은 화면상 1팀, 1은 화면상 2팀이다. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Team Movement",
		meta = (ClampMin = "0", ClampMax = "1"))
	int32 TeamId = 0;

	/** 활성 상태에서 시작 위치에 더할 로컬 이동량이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team Movement")
	FVector MoveOffset = FVector::ZeroVector;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Team Movement",
		meta = (ClampMin = "0.0", Units = "s"))
	float MoveDuration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team Visual")
	FName TeamColorParameterName = TEXT("Paint Tint");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team Visual")
	FLinearColor Team0Color = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team Visual")
	FLinearColor Team1Color = FLinearColor::Blue;

private:
	FVector InactiveLocation = FVector::ZeroVector;
	FVector MoveStartLocation = FVector::ZeroVector;
	FVector MoveTargetLocation = FVector::ZeroVector;
	float MoveElapsedTime = 0.f;
	bool bMoving = false;
	bool bLocationInitialized = false;
};
