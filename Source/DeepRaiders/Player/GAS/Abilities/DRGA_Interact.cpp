// Fill out your copyright notice in the Description page of Project Settings.


#include "DRGA_Interact.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Core/Interaction/DRInteractionTypes.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DeepRaiders/Player/Components/DRInteractionComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "GameplayPrediction.h"
#include "GameFramework/Pawn.h"

UDRGA_Interact::UDRGA_Interact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UDRGA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (ActorInfo == nullptr)
	{
		return;
	}
	
	if (ActorInfo->IsNetAuthority()
		&& !ActorInfo->IsLocallyControlled())
	{
		if (!RegisterTargetDataDelegate())
		{
			EndAbility(Handle,ActorInfo, ActivationInfo, true, true);
		}
		
		return;
	}
	
	if (ActorInfo->IsLocallyControlled())
	{
		EDRInteractionValidationResult FailureResult;
		
		if (!SendLocalInteractionAttempt(FailureResult))
		{
			LogInteractionFailure(FailureResult, nullptr);
			
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		}
		
		return;
	}
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void UDRGA_Interact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UnregisterTargetDataDelegate();
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UDRGA_Interact::SendLocalInteractionAttempt(EDRInteractionValidationResult& OutFailureResult)
{
	OutFailureResult= EDRInteractionValidationResult::NoFocusedTarget;
	
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	
	if (ActorInfo == nullptr
		|| !ActorInfo->IsLocallyControlled())
	{
		OutFailureResult = EDRInteractionValidationResult::InvalidInteractor;
		return false;
	}
	
	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(ActorInfo->PlayerController.Get());

	const UDRInteractionComponent* InteractionComponent = IsValid(PlayerController)	?
		PlayerController->GetInteractionComponent()	: nullptr;

	if (!IsValid(InteractionComponent))
	{
		OutFailureResult = EDRInteractionValidationResult::MissingInteractionComponent;
		return false;
	}

	AActor* Target = InteractionComponent->GetFocusedTarget();

	if (!IsValid(Target))
	{
		return false;
	}
	
	FGameplayAbilityTargetData_ActorArray* ActorData = new FGameplayAbilityTargetData_ActorArray();
	
	ActorData->TargetActorArray.Add(Target);
	FGameplayAbilityTargetDataHandle TargetData(ActorData);
	
	if (ActorInfo->IsNetAuthority())
	{
		HandleServerTargetData(TargetData, FGameplayTag());
		return true;
	}
	
	UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
	
	if (!IsValid(AbilitySystem))
	{
		OutFailureResult = EDRInteractionValidationResult::InvalidInteractor;
		return false;
	}
	
	FScopedPredictionWindow PredictionWindow(
		AbilitySystem,
		true);
	
	AbilitySystem->CallServerSetReplicatedTargetData(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActivationInfo().GetActivationPredictionKey(),
		TargetData, FGameplayTag(), AbilitySystem->ScopedPredictionKey);

	return true;
}

bool UDRGA_Interact::RegisterTargetDataDelegate()
{
	if (TargetDataDelegateHandle.IsValid())
	{
		return true;
	}

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();

	if (!IsValid(AbilitySystem))
	{
		return false;
	}

	const FGameplayAbilitySpecHandle SpecHandle = GetCurrentAbilitySpecHandle();

	const FPredictionKey PredictionKey = GetCurrentActivationInfo().GetActivationPredictionKey();

	TargetDataDelegateHandle = AbilitySystem->AbilityTargetDataSetDelegate(SpecHandle, PredictionKey)
		.AddUObject(this, &ThisClass::HandleServerTargetData);

	AbilitySystem->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, PredictionKey);

	return true;
}

void UDRGA_Interact::UnregisterTargetDataDelegate()
{
	if (!TargetDataDelegateHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->AbilityTargetDataSetDelegate(
			GetCurrentAbilitySpecHandle(),
			GetCurrentActivationInfo().GetActivationPredictionKey())
			.Remove(TargetDataDelegateHandle);
	}

	TargetDataDelegateHandle.Reset();
}

void UDRGA_Interact::HandleServerTargetData(const FGameplayAbilityTargetDataHandle& TargetData,
	FGameplayTag ApplicationTag)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (ActorInfo == nullptr
		|| !ActorInfo->IsNetAuthority())
	{
		return;
	}

	const FGameplayAbilityTargetDataHandle TargetDataCopy = TargetData;

	if (!ActorInfo->IsLocallyControlled())
	{
		if (UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get())
		{
			AbilitySystem->ConsumeClientReplicatedTargetData(
				GetCurrentAbilitySpecHandle(),
				GetCurrentActivationInfo()
					.GetActivationPredictionKey());
		}
	}

	AActor* Target = nullptr;

	EDRInteractionValidationResult Result = ExtractTargetActor(TargetDataCopy, Target);

	if (Result == EDRInteractionValidationResult::Success)
	{
		Result = ExecuteServerInteraction(Target);
	}

	if (Result != EDRInteractionValidationResult::Success)
	{
		LogInteractionFailure(Result, Target);
	}

	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(),
		true,Result != EDRInteractionValidationResult::Success);
}

EDRInteractionValidationResult UDRGA_Interact::ExtractTargetActor(const FGameplayAbilityTargetDataHandle& TargetData,
	AActor*& OutTarget) const
{
	OutTarget = nullptr;

	if (TargetData.Num() != 1)
	{
		return EDRInteractionValidationResult::InvalidTargetData;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);

	if (Data == nullptr
		|| Data->GetScriptStruct() != FGameplayAbilityTargetData_ActorArray::StaticStruct())
	{
		return EDRInteractionValidationResult::InvalidTargetData;
	}

	const TArray<TWeakObjectPtr<AActor>> TargetActors = Data->GetActors();

	if (TargetActors.Num() != 1
		|| !TargetActors[0].IsValid())
	{
		return EDRInteractionValidationResult::InvalidTargetData;
	}

	OutTarget = TargetActors[0].Get();

	return EDRInteractionValidationResult::Success;
}

EDRInteractionValidationResult UDRGA_Interact::ExecuteServerInteraction(AActor* Target) const
{
	const FGameplayAbilityActorInfo* ActorInfo =
	GetCurrentActorInfo();

	APawn* Interactor = ActorInfo != nullptr ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (!IsValid(Interactor))
	{
		return EDRInteractionValidationResult::InvalidInteractor;
	}

	if (!IsValid(Target))
	{
		return EDRInteractionValidationResult::InvalidTarget;
	}

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(Interactor->GetController());

	UDRInteractionComponent* InteractionComponent = IsValid(PlayerController)
		? PlayerController->GetInteractionComponent() : nullptr;

	if (!IsValid(InteractionComponent))
	{
		return EDRInteractionValidationResult::MissingInteractionComponent;
	}

	const FDRInteractionAttempt Attempt(Interactor, Target);

	EDRInteractionValidationResult Result = InteractionComponent->ValidateInteractionAttempt(Attempt);

	if (Result != EDRInteractionValidationResult::Success)
	{
		return Result;
	}

	if (!IDRInteractableInterface::Execute_CanInteract(Target, Interactor))
	{
		return EDRInteractionValidationResult::InteractionRejected;
	}

	if (!IDRInteractableInterface::Execute_Interact(Target, Interactor))
	{
		return EDRInteractionValidationResult::ExecutionFailed;
	}

	return EDRInteractionValidationResult::Success;
}

void UDRGA_Interact::LogInteractionFailure(EDRInteractionValidationResult Result, AActor* Target) const
{
	const TCHAR* TargetName = IsValid(Target) ? *Target->GetName() : TEXT("None");

	switch (Result)
	{
	case EDRInteractionValidationResult::InvalidTargetData:
	case EDRInteractionValidationResult::TargetNotInteractable:
		UE_LOG(LogDRInteraction, Warning, TEXT("Interaction rejected. Result=%s, Target=%s"),
			LexToString(Result), TargetName);
		break;

	case EDRInteractionValidationResult::InvalidInteractor:
	case EDRInteractionValidationResult::MissingInteractionComponent:
	case EDRInteractionValidationResult::ExecutionFailed:
		UE_LOG(LogDRInteraction, Error, TEXT("Interaction failed. Result=%s, Target=%s"),
			LexToString(Result), TargetName);
		break;

	default:
		UE_LOG(LogDRInteraction, Verbose, TEXT("Interaction rejected. Result=%s, Target=%s"),
			LexToString(Result), TargetName);
		break;
	}
}
