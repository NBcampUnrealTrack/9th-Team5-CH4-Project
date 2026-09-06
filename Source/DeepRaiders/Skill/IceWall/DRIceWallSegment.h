#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "DRIceWallSegment.generated.h"

class AController;

/** 얼음벽을 구성하는 파괴 가능한 육각 조각이다. 설치 팀의 공격은 받지 않는다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRIceWallSegment : public ADRBreakableActor
{
	GENERATED_BODY()

public:
	/** 서버가 Deferred Spawn 중 설치 팀과 조각 체력을 설정한다. */
	void InitializeSegment(int32 InOwnerTeamId, float InMaxHealth, const FVector& InDimensions);

	/** GA가 계산한 물리 크기를 적용한다. 조각 Blueprint는 메시와 연출만 정의한다. */
	void SetSegmentDimensions(const FVector& InDimensions);

	UFUNCTION(BlueprintPure, Category = "Ice Wall")
	int32 GetOwnerTeamId() const { return OwnerTeamId; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
		AActor* DamageCauser) override;

private:
	virtual void BeginPlay() override;

	bool CanReceiveDamageFrom(const AController* EventInstigator) const;
	void ApplySegmentDimensions();

	UFUNCTION()
	void OnRep_SegmentDimensions();

	/** 설치한 팀. 유효하지 않은 공격자/팀은 얼음벽에 피해를 주지 못한다. */
	UPROPERTY(Replicated)
	int32 OwnerTeamId = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_SegmentDimensions)
	FVector SegmentDimensions = FVector::OneVector;
};
