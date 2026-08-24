#pragma once

#include "CoreMinimal.h"
#include "DRInteractionTypes.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRInteractionPromptData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FText ActionText;
	
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FText TitleText;
	
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FText DetailText;
	
	bool operator==(const FDRInteractionPromptData& Other) const
	{
		return ActionText.EqualTo(Other.ActionText)
			&& TitleText.EqualTo(Other.TitleText)
			&& DetailText.EqualTo(Other.DetailText);
	}
	
	bool operator!=(const FDRInteractionPromptData& Other) const
	{
		return !(*this == Other);
	}
};

UENUM(BlueprintType)
enum class EDRInteractionValidationResult : uint8
{
	Success,
	NoFocusedTarget,
	InvalidTargetData,
	InvalidInteractor,
	InvalidTarget,
	MissingInteractionComponent,
	TargetNotInteractable,
	PromptUnavailable,
	OutOfRange,
	OutsideInteractionAngle,
	BlockedLineOfSight,
	InteractionRejected,
	ExecutionFailed
};

/*
 * 서버 검증 함수에 전달되는 일시적인 요청 정보
 * 네트워크로 직렬화하지 않고, 서버에서 TargetData를 해석한 뒤 생성
 */
struct DEEPRAIDERS_API FDRInteractionAttempt
{
	APawn* Interactor = nullptr;
	AActor* Target = nullptr;
	
	FDRInteractionAttempt() = default;
	
	FDRInteractionAttempt(APawn* InInteractor, AActor* InTarget)
		: Interactor(InInteractor), Target(InTarget)
	{
	}
};

DECLARE_LOG_CATEGORY_EXTERN(LogDRInteraction, Log, All);

DEEPRAIDERS_API const TCHAR* LexToString(EDRInteractionValidationResult Result);
