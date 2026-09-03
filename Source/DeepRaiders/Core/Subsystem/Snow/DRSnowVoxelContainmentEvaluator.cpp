#include "DRSnowVoxelContainmentEvaluator.h"

#include "DRSnowSurfaceEditor.h"
#include "DeepRaiders/Player/Components/DRVoxelContainmentComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "VoxelWorld.h"

void FDRSnowVoxelContainmentEvaluator::EvaluateCharactersInEditedBounds(
	AVoxelWorld& VoxelWorld,
	const FVoxelIntBox& EditedBounds) const
{
	UWorld* World = VoxelWorld.GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client || !EditedBounds.IsValid())
	{
		return;
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

	if (!EditedWorldBounds.IsValid)
	{
		return;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
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
		}
	}
}

void FDRSnowVoxelContainmentEvaluator::EvaluateSurfaceEdit(
	const FDRSnowSurfaceEditResult& EditResult) const
{
	AVoxelWorld* VoxelWorld = EditResult.VoxelWorld.Get();
	if (EditResult.AppliedAmount <= 0.f ||
		!IsValid(VoxelWorld) || !VoxelWorld->IsCreated() ||
		!EditResult.EditedBounds.IsValid())
	{
		return;
	}

	EvaluateCharactersInEditedBounds(*VoxelWorld, EditResult.EditedBounds);
}
