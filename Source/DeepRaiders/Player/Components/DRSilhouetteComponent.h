#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRSilhouetteComponent.generated.h"

class UPrimitiveComponent;

struct FDRSearchRevealState
{
	TWeakObjectPtr<UPrimitiveComponent> Component;
	bool IsRenderCustomDepthEnabled = false;
	int32 StencilValue = 0;
};

struct FDRSearchRevealRequest
{
	double EndTime = 0.0;
	int32 StencilValue = 0;
};

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRSilhouetteComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRSilhouetteComponent();

	void RefreshTeamSilhouette();
	void StartSearchReveal(FGuid RevealId, float Duration, int32 StencilValue);
	void StopSearchReveal(FGuid RevealId);

protected:
	virtual void BeginPlay() override;

private:
	void CaptureSearchRevealState();
	void ApplySearchReveal();
	void RestoreSearchRevealState();
	void RefreshSearchRevealTimer();

	UFUNCTION()
	void HandleSearchRevealExpired();

	static constexpr int32 TeamStencilValue = 2;
	static constexpr int32 BlockerStencilValue = 3;

	TArray<FDRSearchRevealState> SearchRevealStates;
	TMap<FGuid, FDRSearchRevealRequest> SearchRevealRequests;
	FTimerHandle SearchRevealTimerHandle;
};
