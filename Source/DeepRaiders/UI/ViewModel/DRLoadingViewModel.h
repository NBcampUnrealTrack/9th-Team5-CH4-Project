#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "DRLoadingViewModel.generated.h"

UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRLoadingViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetProgress(float InProgress);
	void SetLoadingMessage(const FString& InMessage);

protected:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Loading")
	float Progress = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Loading")
	FString LoadingMessage;
};
