
#include "DRInteractionComponent.h"

#include "AbilitySystemComponent.h"
#include "DeepRaiders/Player/DRPlayerController.h"
#include "DeepRaiders/Core/Collision/DRCollisionChannels.h"
#include "DeepRaiders/GameplayTags/DRGameplayTags.h"
#include "DeepRaiders/Core/Interface/DRInteractableInterface.h"
#include "DeepRaiders/Shop/DRShop.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemBlueprintLibrary.h"


UDRInteractionComponent::UDRInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	
	SetIsReplicatedByDefault(false);
}

void UDRInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// 클라이언트 한정으로 Tick 이벤트를 발생시킨다.
	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	SetComponentTickEnabled(IsValid(PlayerController) && PlayerController->IsLocalController());
}

void UDRInteractionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	APawn* Interactor	= nullptr;
	
	if(!CanUpdateLocalFocus(Interactor))
	{
		ClearFocusedTarget();
		return;
	}
	
	FDRInteractionPromptData PromptData;
	AActor* Target = FindBestInteractionTarget(Interactor, PromptData);
	
	SetFocusedTarget(Target, PromptData);
}

bool UDRInteractionComponent::CanUpdateLocalFocus(APawn*& OutInteractor) const
{
	OutInteractor = nullptr;
	
	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| PlayerController->bShowMouseCursor)
	{
		return false;
	}
	
	OutInteractor = PlayerController->GetPawn();
	if (!IsValid(OutInteractor))
	{
		return false;
	}
	
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	
	return IsValid(AbilitySystem)
		&& !AbilitySystem->HasMatchingGameplayTag(DRGameplayTags::State_Dead);
	
}

AActor* UDRInteractionComponent::FindBestInteractionTarget(APawn* Interactor,
	FDRInteractionPromptData& OutPromptData) const
{
	OutPromptData = FDRInteractionPromptData();
	
	if (!IsValid(Interactor)|| MaxInteractionDistance <= 0.f)
	{
		return nullptr;
	}
	
	UWorld* World = Interactor->GetWorld();
	
	if (!IsValid(World))
	{
		return nullptr;
	}
		
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRInteractOverlap), false, Interactor);
	
	// 상호작용 반경 내의 오브젝트 오버랩 검사
	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByChannel(OverlapResults, Interactor->GetActorLocation(),
		FQuat::Identity, DRCollisionChannels::Interaction,
		FCollisionShape::MakeSphere(MaxInteractionDistance), QueryParams);
	
	AActor* BestTarget = nullptr;
	float BestAimDot = -1.f;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	FDRInteractionPromptData BestPromptData;
	
	AActor* CurrentTarget = FocusedTarget.Get();
	float CurrentAimDot = -1.f;
	float CurrentDistanceSquared = TNumericLimits<float>::Max();
	FDRInteractionPromptData CurrentPromptData;
	bool bCurrentTargetValid = false;
	
	TSet<AActor*> EvaluatedActors;
	
	// 반경 내의 오브젝트 중 Aim과 가장 근접한 상호작용 액터 탐색
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* Candidate = OverlapResult.GetActor();
		
		if (!IsValid(Candidate)
			|| Candidate == Interactor
			|| EvaluatedActors.Contains(Candidate))
		{
			continue;
		}
		
		EvaluatedActors.Add(Candidate);
		
		float AimDot = -1.f;
		float DistanceSquared = TNumericLimits<float>::Max();
		FDRInteractionPromptData PromptData;
		
		const EDRInteractionValidationResult Result = EvaluateInteractionTarget(
			Interactor, Candidate, AimDot, DistanceSquared, &PromptData);
		
		if (Result != EDRInteractionValidationResult::Success)
		{
			continue;
		}
		
		if (Candidate == CurrentTarget)
		{
			bCurrentTargetValid = true;
			CurrentAimDot = AimDot;
			CurrentDistanceSquared = DistanceSquared;
			CurrentPromptData = PromptData;
		}
		
		const bool bCloserToCrosshair = AimDot > BestAimDot + KINDA_SMALL_NUMBER;
		const bool bSameAngleButCloser = FMath::IsNearlyEqual(AimDot, BestAimDot, KINDA_SMALL_NUMBER)
			&& DistanceSquared < BestDistanceSquared;
		const bool IsCandidateShop = Candidate->IsA<ADRShop>();
		const bool IsBestTargetShop = IsValid(BestTarget) && BestTarget->IsA<ADRShop>();
		
		if (!IsValid(BestTarget)
			|| (!IsCandidateShop && IsBestTargetShop)
			|| (IsCandidateShop == IsBestTargetShop && (bCloserToCrosshair || bSameAngleButCloser)))
		{
			BestTarget = Candidate;
			BestAimDot = AimDot;
			BestDistanceSquared = DistanceSquared;
			BestPromptData = PromptData;
		}
	}
	
	if (bCurrentTargetValid
		&& IsValid(BestTarget)
		&& BestTarget != CurrentTarget
		&& CurrentTarget->IsA<ADRShop>() == BestTarget->IsA<ADRShop>())
	{
		const float CurrentAngleDegrees = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(CurrentAimDot, -1.f, 1.f)));
		const float BestAngleDegrees =  FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(BestAimDot, -1.f, 1.f)));
		
		/* 보정치를 두어 너무 빈번하게 Focus 대상이 바뀌지 않도록 한다.
		 * 현재 Focus된 대상을 우선시 한다.
		 */
		const bool bHasEnoughAdvantage = BestAngleDegrees + FocusSwitchAngleAdvantageDegrees < CurrentAngleDegrees;
		if (!bHasEnoughAdvantage)
		{
			OutPromptData = CurrentPromptData;
			return CurrentTarget;
		}
	}
	
	OutPromptData = BestPromptData;
	return BestTarget;
}

EDRInteractionValidationResult UDRInteractionComponent::EvaluateInteractionTarget(APawn* Interactor, AActor* Target,
	float& OutAimDot, float& OutDistanceSquared, FDRInteractionPromptData* OutPromptData) const
{
	OutAimDot = -1.f;
	OutDistanceSquared = TNumericLimits<float> ::Max();
	
	if (!IsValid(Interactor))
	{
		return EDRInteractionValidationResult::InvalidInteractor;
	}
	
	if (!IsValid(Target) 
		|| Target == Interactor)
	{
		return EDRInteractionValidationResult::InvalidTarget;
	}

	if (!Target->Implements<UDRInteractableInterface>())
	{
		return EDRInteractionValidationResult::TargetNotInteractable;
	}
	
	FDRInteractionPromptData PromptData;
	if (!IDRInteractableInterface::Execute_GetInteractionPromptData(Target, Interactor, PromptData))
	{
		return EDRInteractionValidationResult::PromptUnavailable;
	}
	
	if (PromptData.ActionText.IsEmpty())
	{
		PromptData.ActionText = NSLOCTEXT("DRInteraction", "DefaultInteractionAction", "상호작용");
	}
	
	UWorld* World = Interactor->GetWorld();
	AController* Controller = Interactor->GetController();

	if (!IsValid(World) 
		|| !IsValid(Controller))
	{
		return EDRInteractionValidationResult::InvalidInteractor;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	
	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();

	if (ViewDirection.IsNearlyZero())
	{
		return EDRInteractionValidationResult::InvalidInteractor;
	}

	FVector InteractionLocation = FVector::ZeroVector;

	const bool bHasCustomInteractionLocation =
		IDRInteractableInterface::Execute_GetInteractionLocation(
			Target,
			Interactor,
			InteractionLocation);

	if (!bHasCustomInteractionLocation
		|| InteractionLocation.ContainsNaN())
	{
		FVector BoundsExtent;
		Target->GetActorBounds(false, InteractionLocation, BoundsExtent);
	}
	
	const FVector PawnToTarget =
		InteractionLocation - Interactor->GetActorLocation();

	OutDistanceSquared = PawnToTarget.SizeSquared();

	if (OutDistanceSquared <= KINDA_SMALL_NUMBER
		|| OutDistanceSquared > FMath::Square(MaxInteractionDistance))
	{
		return EDRInteractionValidationResult::OutOfRange;
	}

	const FVector ViewToTargetDirection =
		(InteractionLocation - ViewLocation).GetSafeNormal();

	OutAimDot =
		FVector::DotProduct(
			ViewDirection,
			ViewToTargetDirection);

	const float MinimumAimDot =
		FMath::Cos(
			FMath::DegreesToRadians(
				MaxInteractionAngleDegrees));

	if (OutAimDot < MinimumAimDot)
	{
		return EDRInteractionValidationResult::OutsideInteractionAngle;
	}

	if (!HasClearLineOfSight(
			World,
			Interactor,
			Target,
			ViewLocation,
			InteractionLocation))
	{
		return EDRInteractionValidationResult::BlockedLineOfSight;
	}

	if (OutPromptData != nullptr)
	{
		*OutPromptData = PromptData;
	}

	return EDRInteractionValidationResult::Success;
}

EDRInteractionValidationResult UDRInteractionComponent::ValidateInteractionAttempt(
	const FDRInteractionAttempt& Attempt) const
{
	if (!IsValid(Attempt.Interactor))
	{
		return EDRInteractionValidationResult::InvalidInteractor;
	}

	if (!IsValid(Attempt.Target))
	{
		return EDRInteractionValidationResult::InvalidTarget;
	}

	const ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwner());

	if (!IsValid(PlayerController)
		|| PlayerController->GetPawn() != Attempt.Interactor)
	{
		return EDRInteractionValidationResult::InvalidInteractor;
	}

	float AimDot = -1.f;
	float DistanceSquared = TNumericLimits<float>::Max();

	return EvaluateInteractionTarget(Attempt.Interactor, Attempt.Target, AimDot, DistanceSquared, nullptr);
}

bool UDRInteractionComponent::HasClearLineOfSight(UWorld* World, APawn* Interactor, AActor* Target,
	const FVector& ViewLocation, const FVector& TargetLocation) const
{
	if (!IsValid(World) 
		|| !IsValid(Interactor) 
		|| !IsValid(Target))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRInteractLineOfSight), true, Interactor);

	FHitResult HitResult;
	const bool bBlockingHit = World->LineTraceSingleByChannel(HitResult, ViewLocation, TargetLocation,
	                                                          ECC_Visibility, QueryParams);

	return !bBlockingHit || HitResult.GetActor() == Target;
}

void UDRInteractionComponent::SetFocusedTarget(AActor* NewTarget, const FDRInteractionPromptData& NewPromptData)
{
	const bool bTargetChanged = FocusedTarget.Get() != NewTarget;
	const bool bPromptChanged = FocusedPromptData != NewPromptData;

	if (!bTargetChanged && !bPromptChanged)
	{
		return;
	}

	FocusedTarget = NewTarget;
	FocusedPromptData = NewPromptData;

	OnFocusedInteractableChanged.Broadcast(NewTarget, FocusedPromptData);
}

void UDRInteractionComponent::ClearFocusedTarget()
{
	SetFocusedTarget(nullptr, FDRInteractionPromptData());
}
