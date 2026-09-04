
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "DRProjectileTypes.h"
#include "DRProjectile.generated.h"

class UAbilitySystemComponent;
class UProjectileMovementComponent;
class UShapeComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRProjectile : public AActor
{
	GENERATED_BODY()
	
public:
	ADRProjectile(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	/** Ballistic Aim 계산에서 사용하는 이 Projectile의 실제 기본 발사 속도. */
	virtual float GetConfiguredInitialSpeed() const;

	/** Ballistic Aim 계산에서 사용하는 ProjectileGravityScale. */
	virtual float GetConfiguredGravityScale() const;

	/** Deferred Spawn 중 계산된 초기 발사 속도를 BeginPlay 전에 전달한다. */
	void SetInitialLaunchVelocity(const FVector& InLaunchVelocity);
	
	// 서버에서 Projectile Spawn을 완료하기 전에 반드시 호출
	void InitializeProjectile(
		UAbilitySystemComponent* InSourceAbilitySystem,
		const TArray<FGameplayEffectSpecHandle>& InImpactEffectSpecs,
		float InBreakableDamageAmount,
		const FDRProjectileWorldImpactData& InWorldImpactData,
		int32 InSourceTeamId,
		const UObject* InPresentationSourceObject,
		float InEffectiveMaxRange = 0.f,
		const FDRProjectileFalloffSettings& InFalloffSettings = FDRProjectileFalloffSettings());
	
protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	
	// ProjectileMovement가 Blocking Hit로 정지했을 때
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);
	
	// 실제 Projectile 충돌 처리.
	// Cannon은 override해서 Explosion 처리.
	virtual void HandleImpact(const FHitResult& ImpactResult);
	
	// 플레이어 적중 Effect를 서버에서 적용
	void ApplyImpactEffect(UAbilitySystemComponent* TargetAbilitySystem, const FHitResult& ImpactResult);
	
	// Projectile Effect가 적용된 플레이어에게 피격 Presentation Cue를 실행한다.
	void ExecutePlayerHitGameplayCue(UAbilitySystemComponent* TargetAbilitySystem, const FHitResult& ImpactResult);
	
	bool ApplyBreakableDamage(const FHitResult& ImpactResult);
	
	// 같은 팀인지 검사
	bool IsFriendlyTarget(const AActor* TargetActor) const;
	
	// 월드 충돌 처리
	virtual void HandleWorldImpact(const FHitResult& ImpactResult);

	const FDRProjectileWorldImpactData& GetWorldImpactData() const { return WorldImpactData; }
	int32 GetSourceTeamId() const {	return SourceTeamId; }
	float GetCurrentFalloffStrength() const { return CurrentFalloffStrength; }
	
	// 아군 충돌 무시 설정
	void RefreshFriendlyCollisionIgnores();
	virtual void ExecuteImpactGameplayCue(const FHitResult& ImpactResult);
	
	static const FName CollisionComponentName;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = true))
	TObjectPtr<UShapeComponent> CollisionComponent;

	UAbilitySystemComponent* GetSourceAbilitySystem() const
	{
		return SourceAbilitySystem.Get();
	}
	
	UObject* GetPresentationSourceObject() const
	{
		return PresentationSourceObject.Get();
	}
	
	void ConfigureProjectileMovement(float InitialSpeed, float GravityScale);
	
	virtual bool ShouldIgnoreFriendlyBlockingHit() const
	{
		return true;
	}

	float EvaluateFalloffStrengthAtLocation(const FVector& Location) const;
	
private:
	void UpdateFalloffAtLocation(const FVector& Location);
	void ApplyFalloffScale(float Strength);
	void ApplySizeMultiplier(float SizeMultiplier);
	void ScaleImpactSetByCallerMagnitude(FGameplayEffectSpec& ImpactSpec, const FGameplayTag& DataTag) const;

	UFUNCTION()
	void OnRep_SizeMultiplier();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = true))
	TObjectPtr<UStaticMeshComponent> MeshComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = true))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
	
	// EffectSpec을 생성한 Source ASC. 서버 전용
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;
	
	TArray<FGameplayEffectSpecHandle> ImpactEffectSpecs;
	
	float BreakableDamageAmount = 0.f;
	
	FDRProjectileWorldImpactData WorldImpactData;
	
	int32 SourceTeamId = INDEX_NONE;
	
	bool bImpactHandled = false;	
	
	TWeakObjectPtr<UObject> PresentationSourceObject;

	UPROPERTY(Transient)
	FDRProjectileFalloffSettings FalloffSettings;

	FVector LaunchLocation = FVector::ZeroVector;
	FVector InitialLaunchVelocity = FVector::ZeroVector;
	FVector InitialActorScale = FVector::OneVector;
	float EffectiveMaxRange = 0.f;
	float CurrentFalloffStrength = 1.f;
	float LastAppliedSizeMultiplier = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_SizeMultiplier)
	uint8 ReplicatedSizeMultiplier = MAX_uint8;
};
