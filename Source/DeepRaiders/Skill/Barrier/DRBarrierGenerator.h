#pragma once

#include "CoreMinimal.h"
#include "DeepRaiders/Core/Interface/DRCombatTeamInterface.h"
#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "DRBarrierGenerator.generated.h"

class ADRPlayerCharacter;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

/** 적의 공격을 체력으로 흡수하는 구형 배리어 생성기다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRBarrierGenerator : public ADRBreakableActor, public IDRCombatTeamInterface
{
	GENERATED_BODY()

public:
	ADRBarrierGenerator();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void Initialize(ADRPlayerCharacter* SourceCharacter, float InBarrierRadius, float InBarrierDuration,
		float InBarrierMaxHealth);
	virtual int32 GetCombatTeamId() const override { return OwnerTeamId; }
	USphereComponent* GetBarrierCollisionComponent() const;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	TObjectPtr<USphereComponent> BarrierCollision;

	/** 물리 투사체와 충돌하지 않고 히트스캔 Trace에만 사용되는 구체다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	TObjectPtr<USphereComponent> BarrierTraceCollision;

	/** BarrierRadius에 맞춰 자동으로 균일 스케일되는 구형 배리어 외형이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	TObjectPtr<UStaticMeshComponent> BarrierVisual;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_BarrierRadius, Category = "Barrier", meta = (Units = "cm"))
	float BarrierRadius = 400.f;
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Barrier", meta = (Units = "s"))
	float BarrierDuration = 8.f;
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Barrier")
	int32 OwnerTeamId = INDEX_NONE;

private:
	UFUNCTION() void OnRep_BarrierRadius();
	UFUNCTION() void HandleBarrierBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);
	/** GA 반경을 Collision과 BarrierVisual에 동일하게 반영한다. */
	void RefreshBarrierGeometry();
};
