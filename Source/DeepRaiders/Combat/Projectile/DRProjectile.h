
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "DRProjectileTypes.h"
#include "DRProjectile.generated.h"

class UAbilitySystemComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRProjectile : public AActor
{
	GENERATED_BODY()
	
public:
	ADRProjectile();
	
	// 서버에서 Projectile Spawn을 완료하기 전에 반드시 호출
	void InitializeProjectile(UAbilitySystemComponent* InSourceAbilitySystem
		, const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs
		, const FDRProjectileWorldImpactData& InWorldImpactData
		, int32 InSourceTeamId);
	
protected:
	virtual void BeginPlay() override;
	
	// ProjectileMovement가 Blocking Hit로 정지했을 때
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);
	
	// 플레이어 적중 Effect를 서버에서 적용
	void ApplyImpactEffect(UAbilitySystemComponent* TargetAbilitySystem, const FHitResult& ImpactResult);
	
	// 같은 팀인지 검사
	bool IsFriendlyTarget(const AActor* TargetActor) const;
	
	// 월드 충돌 처리
	virtual void HandleWorldImpact(const FHitResult& ImpactResult);
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = true))
	TObjectPtr<USphereComponent> CollisionComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = true))
	TObjectPtr<UStaticMeshComponent> MeshComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = true))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
	
	// EffectSpec을 생성한 Source ASC. 서버 전용
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;
	
	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	
	FDRProjectileWorldImpactData WorldImpactData;
	
	int32 SourceTeamId = INDEX_NONE;
	
	bool bImpactHandled = false;	
};
