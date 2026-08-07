#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRTitleGameMode.generated.h"

class UUserWidget;

UCLASS()
class DEEPRAIDERS_API ADRTitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	TSubclassOf<UUserWidget> HostOrJoinWidgetClass;

private:
	UPROPERTY()
	UUserWidget* HostOrJoinWidgetInstance;

public:
	ADRTitleGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
