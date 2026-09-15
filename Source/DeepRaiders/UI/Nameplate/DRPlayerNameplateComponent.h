#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "DRPlayerNameplateComponent.generated.h"

class ADRPlayerCharacter;
class ADRPlayerController;
class ADRPlayerState;
class UDRPlayerNameplateViewModel;

UCLASS(ClassGroup = UI, meta = (BlueprintSpawnableComponent))
class DEEPRAIDERS_API UDRPlayerNameplateComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UDRPlayerNameplateComponent();

	void RefreshDisplayData();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RefreshVisibility();

	bool HasNameplateLineOfSight(const ADRPlayerController* LocalController, const ADRPlayerCharacter* TargetCharacter) const;

	bool IsTracePointVisible(const FVector& ViewLocation, const FVector& TargetPoint, const FCollisionQueryParams& QueryParams) const;

	UPROPERTY(EditDefaultsOnly, Category = "Nameplate|Visibility", meta = ( ClampMin = "0.02", Units = "s"))
	float VisibilityRefreshInterval = 0.1f;

	/** Capsule 상단에서 이름표까지 추가 간격. */
	UPROPERTY(EditDefaultsOnly, Category = "Nameplate|Layout", meta = (Units = "cm"))
	float HeightPadding = 25.f;

	/** Capsule 중심에서 위쪽으로 검사할 머리 지점 비율. */
	UPROPERTY(EditDefaultsOnly, Category = "Nameplate|Visibility", meta = ( ClampMin = "0.0", ClampMax = "1.0"))
	float HeadPointRatio = 0.75f;

	/** Capsule 중심에서 위쪽으로 검사할 가슴 지점 비율. */
	UPROPERTY(EditDefaultsOnly, Category = "Nameplate|Visibility", meta = ( ClampMin = "0.0", ClampMax = "1.0"))
	float ChestPointRatio = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Nameplate|Visibility")
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Nameplate|MVVM")
	FName ViewModelName = TEXT("DRPlayerNameplateViewModel");

	UPROPERTY(Transient)
	TObjectPtr<UDRPlayerNameplateViewModel> NameplateViewModel;

	FTimerHandle VisibilityRefreshTimer;
};
