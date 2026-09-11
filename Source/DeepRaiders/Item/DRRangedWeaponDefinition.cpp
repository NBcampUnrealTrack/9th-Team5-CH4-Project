#include "DRRangedWeaponDefinition.h"

#include "DeepRaiders/GameplayTags/DRGameplayTags.h"

UDRRangedWeaponDefinition::UDRRangedWeaponDefinition()
{
	Category = EDRItemCategory::Equipment;
	MaxStackSize = 1;

	SnowAbsorbPresentation.StartSoundCueTag = DRGameplayTags::GameplayCue_Sound_Weapon_Absorb_Start;
	SnowAbsorbPresentation.LoopSoundCueTag = DRGameplayTags::GameplayCue_Sound_Weapon_Absorb_Loop;
	SnowAbsorbPresentation.EndSoundCueTag = DRGameplayTags::GameplayCue_Sound_Weapon_Absorb_End;
}

FVector UDRRangedWeaponDefinition::ResolveCameraAimDirection(
	const FVector& ViewDirection,
	const FVector& Origin,
	const FVector& CameraAimPoint) const
{
	const FVector SafeViewDirection = ViewDirection.GetSafeNormal();
	if (SafeViewDirection.IsNearlyZero()
		|| !AimCorrectionSettings.bUseCameraAimCorrection
		|| FVector::Distance(Origin, CameraAimPoint) < AimCorrectionSettings.MinCameraAimCorrectionDistance)
	{
		return SafeViewDirection;
	}

	const FVector CameraAimDirection = (CameraAimPoint - Origin).GetSafeNormal();
	if (CameraAimDirection.IsNearlyZero())
	{
		return SafeViewDirection;
	}

	const float DirectionDot = FMath::Clamp(
		FVector::DotProduct(SafeViewDirection, CameraAimDirection), -1.0f, 1.0f);
	const float CorrectionAngleRadians = FMath::Acos(DirectionDot);
	const float MaxCorrectionAngleRadians = FMath::DegreesToRadians(
		FMath::Clamp(AimCorrectionSettings.MaxCameraAimCorrectionAngleDegrees, 0.0f, 90.0f));

	if (CorrectionAngleRadians <= MaxCorrectionAngleRadians)
	{
		return CameraAimDirection;
	}

	if (CorrectionAngleRadians > KINDA_SMALL_NUMBER && MaxCorrectionAngleRadians > 0.0f)
	{
		const FQuat CorrectionRotation = FQuat::FindBetweenNormals(SafeViewDirection, CameraAimDirection);
		return FQuat::Slerp(
			FQuat::Identity,
			CorrectionRotation,
			MaxCorrectionAngleRadians / CorrectionAngleRadians).RotateVector(SafeViewDirection).GetSafeNormal();
	}

	return SafeViewDirection;
}
