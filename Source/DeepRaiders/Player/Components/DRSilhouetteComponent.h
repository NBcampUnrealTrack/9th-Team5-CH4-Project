#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRSilhouetteComponent.generated.h"

UCLASS(ClassGroup = (DeepRaiders), meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRSilhouetteComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRSilhouetteComponent();

	void RefreshTeamSilhouette();

protected:
	virtual void BeginPlay() override;

private:
	static constexpr int32 TeamStencilValue = 2;
	static constexpr int32 BlockerStencilValue = 3;
};
