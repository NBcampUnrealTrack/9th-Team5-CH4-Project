#pragma once

#include "CoreMinimal.h"
#include "DRTitlePlayerController.generated.h"

UCLASS()
class DEEPRAIDERS_API ADRTitlePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	TSubclassOf<UUserWidget> HostOrJoinWidgetClass;

private:
	UPROPERTY()
	UUserWidget* HostOrJoinWidgetInstance;

protected:
	virtual void BeginPlay() override;

};
