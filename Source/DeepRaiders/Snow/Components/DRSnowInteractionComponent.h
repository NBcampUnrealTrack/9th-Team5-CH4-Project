#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "Engine/HitResult.h"
#include "DRSnowInteractionComponent.generated.h"

// 눈 상호작용 컴포넌트들이 공유하는 팀/소유자 context 기반 클래스다.
UCLASS(
	ClassGroup = (Snow),
	BlueprintType,
	Abstract)
class DEEPRAIDERS_API UDRSnowInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRSnowInteractionComponent();

	UFUNCTION(BlueprintCallable, Category = "Snow|Interaction")
	FDRSnowInteractionContext MakeInteractionContext() const;

	UFUNCTION(BlueprintCallable, Category = "Snow|Interaction")
	void SetTeamIdOverride(int32 InTeamId);

	UFUNCTION(BlueprintCallable, Category = "Snow|Interaction")
	void ClearTeamIdOverride();

protected:
	AActor* GetInteractableActorFromHit(const FHitResult& HitResult) const;

	int32 ResolveTeamId() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow|Interaction")
	int32 TeamIdOverride = INDEX_NONE;
};
