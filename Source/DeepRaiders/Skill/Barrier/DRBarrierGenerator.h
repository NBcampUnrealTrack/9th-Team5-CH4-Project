#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "DeepRaiders/Core/Interface/DRCombatTeamInterface.h"
#include "DeepRaiders/Gameplay/Breakable/DRBreakableActor.h"
#include "GameplayEffectTypes.h"
#include "DRBarrierGenerator.generated.h"

class ADRPlayerCharacter;
class UAbilitySystemComponent;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

/** 적의 공격을 체력으로 흡수하는 배리어 생성기다. */
UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRBarrierGenerator : public ADRBreakableActor, public IDRCombatTeamInterface
{
	GENERATED_BODY()

public:
	ADRBarrierGenerator();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void Initialize(ADRPlayerCharacter* SourceCharacter, float InBarrierRadius, float InBarrierDuration,
		float InBarrierMaxHealth, const TArray<FGameplayEffectSpecHandle>& InAreaEffectSpecs);
	virtual int32 GetCombatTeamId() const override { return OwnerTeamId; }
	virtual UPrimitiveComponent* GetBarrierCollisionComponent() const;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	TObjectPtr<USphereComponent> BarrierCollision;

	/** 물리 투사체와 충돌하지 않고 히트스캔 Trace에만 사용되는 구체다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	TObjectPtr<USphereComponent> BarrierTraceCollision;

	/** 전달된 GameplayEffectSpec들을 적에게 적용하는 방벽 내부 판정 영역이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier|Effects")
	TObjectPtr<USphereComponent> AreaEffectCollision;

	/** BarrierRadius에 맞춰 자동으로 균일 스케일되는 구형 배리어 외형이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	TObjectPtr<UStaticMeshComponent> BarrierVisual;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_BarrierRadius, Category = "Barrier", meta = (Units = "cm"))
	float BarrierRadius = 400.f;
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Barrier", meta = (Units = "s"))
	float BarrierDuration = 8.f;
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Barrier")
	int32 OwnerTeamId = INDEX_NONE;

	UFUNCTION() void HandleBarrierBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool IsFromSweep,
		const FHitResult& SweepResult);
	/** GA 반경을 Collision과 BarrierVisual에 동일하게 반영한다. */
	virtual void RefreshBarrierGeometry();

private:
	UFUNCTION() void OnRep_BarrierRadius();
	UFUNCTION() void HandleAreaEffectBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);
	UFUNCTION() void HandleAreaEffectEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
	void ApplyAreaEffects(AActor* TargetActor);
	void RemoveAreaEffects(AActor* TargetActor);
	void RemoveAllAreaEffects();
	UAbilitySystemComponent* GetTargetAbilitySystem(AActor* TargetActor) const;

	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;
	TArray<FGameplayEffectSpecHandle> AreaEffectSpecs;
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, TArray<FActiveGameplayEffectHandle>> ActiveAreaEffects;
};
