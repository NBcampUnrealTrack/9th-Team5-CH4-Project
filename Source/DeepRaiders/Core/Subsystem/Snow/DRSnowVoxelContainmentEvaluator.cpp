#include "DRSnowVoxelContainmentEvaluator.h"

#include "DRSnowSurfaceEditor.h"
#include "DeepRaiders/Player/Components/DRVoxelContainmentComponent.h"
#include "DeepRaiders/Snow/DRSnowTypes.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "VoxelWorld.h"

bool FDRSnowVoxelContainmentEvaluator::EvaluateCharactersInEditedBounds(
	AVoxelWorld& VoxelWorld,
	const FVoxelIntBox& EditedBounds) const
{
	if (!EditedBounds.IsValid())
	{
		return false;
	}

	FBox EditedWorldBounds(ForceInit);
	const FIntVector Min = EditedBounds.Min;
	const FIntVector Max = EditedBounds.Max;
	for (int32 X = 0; X < 2; ++X)
	{
		for (int32 Y = 0; Y < 2; ++Y)
		{
			for (int32 Z = 0; Z < 2; ++Z)
			{
				EditedWorldBounds += VoxelWorld.LocalToGlobal(FIntVector(
					X == 0 ? Min.X : Max.X,
					Y == 0 ? Min.Y : Max.Y,
					Z == 0 ? Min.Z : Max.Z));
			}
		}
	}
	EditedWorldBounds = EditedWorldBounds.ExpandBy(VoxelWorld.VoxelSize);

	bool bEvaluatedAnyCharacter = false;
	if (!EditedWorldBounds.IsValid)
	{
		return bEvaluatedAnyCharacter;
	}

	for (TActorIterator<ACharacter> It(VoxelWorld.GetWorld()); It; ++It)
	{
		ACharacter* Character = *It;
		const UCapsuleComponent* Capsule =
			IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
		if (!IsValid(Capsule) ||
			!EditedWorldBounds.Intersect(Capsule->Bounds.GetBox()))
		{
			continue;
		}

		if (UDRVoxelContainmentComponent* Containment =
			Character->FindComponentByClass<UDRVoxelContainmentComponent>())
		{
			Containment->EvaluateVoxelContainment(&VoxelWorld);
			bEvaluatedAnyCharacter = true;
		}
	}

	return bEvaluatedAnyCharacter;
}

void FDRSnowVoxelContainmentEvaluator::EvaluateAffectedAdd(
	const FDRSnowSurfaceAddRequest& Request,
	const FDRSnowSurfaceEditResult& EditResult) const
{
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (!IsValid(VoxelWorld) || !VoxelWorld->IsCreated())
	{
		return;
	}

	const bool bEvaluatedAnyCharacter =
		EvaluateCharactersInEditedBounds(*VoxelWorld, EditResult.EditedBounds);
	if (bEvaluatedAnyCharacter)
	{
		return;
	}

	APawn* InstigatorPawn = Request.Context.InstigatorPawn.Get();
	if (!IsValid(InstigatorPawn))
	{
		return;
	}

	if (UDRVoxelContainmentComponent* Containment =
		InstigatorPawn->FindComponentByClass<UDRVoxelContainmentComponent>())
	{
		Containment->EvaluateVoxelContainment(VoxelWorld);
	}
}
