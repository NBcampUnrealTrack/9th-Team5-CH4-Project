#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRKillFeedWidget.generated.h"

class ADRPlayerController;

UCLASS()
class DEEPRAIDERS_API UDRKillFeedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeKillFeed(ADRPlayerController* InPlayerController);

protected:
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Kill Feed")
	void AddKillFeedEntry(
		const FString& KillerName,
		const FString& VictimName);

private:
	UFUNCTION()
	void HandleKillFeedEntry(
		FString KillerName,
		FString VictimName);

	TWeakObjectPtr<ADRPlayerController> PlayerController;
};