#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRMiningComponent.generated.h"

class ADRCNPlayerCharacter;

UCLASS(
	ClassGroup = (Mining),
	BlueprintType,
	Blueprintable,
	meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRMiningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRMiningComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Mining")
	void TryMine();

protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestMine(
		FVector_NetQuantize TraceStart,
		FVector_NetQuantize TraceEnd);

	bool CanMine() const;
	bool IsMineOnCooldown() const;
	bool PerformMiningTrace(FHitResult& OutHitResult) const;
	bool MineLocal(const FHitResult& HitResult) const;
	void GetTraceViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

protected:
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Trace",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MineTraceDistance = 500.f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Voxel",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MineRadius = 150.f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Timing",
		meta = (ClampMin = "0.0", Units = "s"))
	float MineCooldown = 0.25f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Mining|Debug")
	bool bDrawDebugTrace = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<ADRCNPlayerCharacter> OwnerCharacter;

	float LastMineTime = -BIG_NUMBER;
};
