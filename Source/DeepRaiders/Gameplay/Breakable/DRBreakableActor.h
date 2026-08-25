#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRBreakableActor.generated.h"

class AController;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRBreakableDamageContext
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "Breakable")
	float AppliedDamage = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Breakable")
	TObjectPtr<AController> InstigatorController = nullptr;
	
	UPROPERTY(BlueprintReadOnly, Category = "Breakable")
	TObjectPtr<AActor> DamageCauser = nullptr;
	
	UPROPERTY(BlueprintReadOnly, Category = "Breakable")
	FHitResult HitResult;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRBreakableBrokenSignature, const FDRBreakableDamageContext&, DamageContext);

UCLASS(Blueprintable)
class DEEPRAIDERS_API ADRBreakableActor : public AActor
{
	GENERATED_BODY()
	
public:
	ADRBreakableActor();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	
	UFUNCTION(BlueprintPure, Category = "Breakable")
	bool IsBroken() const
	{
		return bIsBroken;
	}
	
	/*
	 * 서버에서 파괴가 확정됐을 때 호출
	 * 클라이언트 연출용 이벤트가 아님.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Breakable")
	FDRBreakableBrokenSignature OnBroken;
	
protected:
	virtual void BeginPlay() override;
	
	// 서버에서 파괴가 확정된 후 자식 클래스가 실행할 Gameplay 처리
	virtual void HandleBroken(const FDRBreakableDamageContext& DamageContext);
	
	// 서버와 클라이언트에서 각각 실행되는 외관 처리
	UFUNCTION(BlueprintImplementableEvent, Category = "Breakable", meta = (DisplayName = "On Broken Presentation"))
	void BP_OnBrokenPresentation();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakable")
	TObjectPtr<UStaticMeshComponent> BreakableMeshComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakable|Health",meta = (ClampMin = "1.0")))
	float MaxHealth = 50.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakable|Presentation")
	bool bHideMeshWhenBroken = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakable|Lifecycle", meta = (ClampMin = "0.0", Units = "s"))
	float BrokenLifeSpan = 1.f;
	
private:
	UFUNCTION()
	void OnRep_IsBroken();
	
	void BreakActor(const FDRBreakableDamageContext& DamageContext);
	void ApplyBrokenPresentation();
	
	float CurrentHealth = 0.f;	
	
	UPROPERTY(ReplicatedUsing = OnRep_IsBroken)
	bool bIsBroken = false;
};


