
#include "DRInteractionTypes.h"

DEFINE_LOG_CATEGORY(LogDRInteraction);

TCHAR const* LexToString(EDRInteractionValidationResult Result)
{
	switch (Result)
	{
	case EDRInteractionValidationResult::Success:
		return TEXT("Success");

	case EDRInteractionValidationResult::NoFocusedTarget:
		return TEXT("NoFocusedTarget");

	case EDRInteractionValidationResult::InvalidTargetData:
		return TEXT("InvalidTargetData");

	case EDRInteractionValidationResult::InvalidInteractor:
		return TEXT("InvalidInteractor");

	case EDRInteractionValidationResult::InvalidTarget:
		return TEXT("InvalidTarget");

	case EDRInteractionValidationResult::MissingInteractionComponent:
		return TEXT("MissingInteractionComponent");

	case EDRInteractionValidationResult::TargetNotInteractable:
		return TEXT("TargetNotInteractable");

	case EDRInteractionValidationResult::PromptUnavailable:
		return TEXT("PromptUnavailable");

	case EDRInteractionValidationResult::OutOfRange:
		return TEXT("OutOfRange");

	case EDRInteractionValidationResult::OutsideInteractionAngle:
		return TEXT("OutsideInteractionAngle");

	case EDRInteractionValidationResult::BlockedLineOfSight:
		return TEXT("BlockedLineOfSight");

	case EDRInteractionValidationResult::InteractionRejected:
		return TEXT("InteractionRejected");

	case EDRInteractionValidationResult::ExecutionFailed:
		return TEXT("ExecutionFailed");

	default:
		return TEXT("Unknown");
	}
}
