#include "DRMeleeCombatComponent.h"

#include "DeepRaiders/Player/DRPlayerCharacter.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AttributesContainer.h"

#include "BonePose.h"
#include "BoneContainer.h"

#include "Components/SkeletalMeshComponent.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Animation/AnimCompositeBase.h"
#include "Misc/MemStack.h"
#include "DeepRaiders/Item/DRMeleeWeaponDefinition.h"

UDRMeleeCombatComponent::UDRMeleeCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	/*
	 * 네트워크 RPC는 GameplayAbility가 담당한다.
	 * 이 Component는 서버의 물리 판정 실행기다.
	 */
	SetIsReplicatedByDefault(false);
}

bool UDRMeleeCombatComponent::StartAttackFromAbility(UDRMeleeWeaponItemDefinition* WeaponDefinition)
{
	if (!CanStartAttackFromAbility(WeaponDefinition))
	{
		return false;
	}

	ActiveWeaponDefinition = WeaponDefinition;

	bIsAttacking = true;
	bHasPreviousSweepSample = false;

	AlreadyHitActors.Reset();

	return true;
}

void UDRMeleeCombatComponent::EndAttackFromAbility()
{
	CancelAttack();
}

bool UDRMeleeCombatComponent::CanStartAttackFromAbility(const UDRMeleeWeaponItemDefinition* WeaponDefinition) const
{
	const ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority() || Character->IsDead() || Character->IsFrozen() || !IsValid(WeaponDefinition))
	{
		return false;
	}

	/*
	 * 현재 실제로 근접 공격 가능한 아이템을
	 * 들고 있는지 서버에서 다시 검증.
	 */
	if (!Character->HasHeldItemAction(EDRItemActionType::MeleeAttack))
	{
		return false;
	}

	return !bIsAttacking;
}

ADRPlayerCharacter* UDRMeleeCombatComponent::GetOwnerCharacter() const
{
	return Cast<ADRPlayerCharacter>(GetOwner());
}

void UDRMeleeCombatComponent::SampleWeaponSweep(UAnimSequenceBase* Animation, const float SampleTime)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority() || !bIsAttacking)
	{
		return;
	}

	const UAnimMontage* Montage = Cast<UAnimMontage>(Animation);

	if (!IsValid(Montage))
	{
		return;
	}

	FVector CurrentBase;
	FVector CurrentTip;

	if (!EvaluateWeaponSweepSample(Montage, SampleTime, CurrentBase, CurrentTip))
	{
		return;
	}

#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GetWorld())
	{
		// 진단용. bDrawDebug 무시하고 무조건 그린다.
		DrawDebugSphere(World, CurrentBase, 20.f, 12, FColor::Cyan, false, 2.f);

		DrawDebugSphere(World, CurrentTip, 20.f, 12, FColor::Magenta, false, 2.f);

		DrawDebugLine(World, CurrentBase, CurrentTip, FColor::Yellow, false, 2.f, 0, 3.f);
	}
#endif

	/*
	 * 첫 번째 고정 Sample.
	 *
	 * 이전 위치가 없으므로
	 * 현재 검날 전체만 검사한다.
	 */
	if (!bHasPreviousSweepSample)
	{
		SweepSegment(CurrentBase, CurrentTip);

		PreviousBaseLocation = CurrentBase;
		PreviousTipLocation = CurrentTip;
		bHasPreviousSweepSample = true;

		return;
	}

	const FVector PreviousMiddle = (PreviousBaseLocation + PreviousTipLocation) * 0.5f;
	const FVector CurrentMiddle = (CurrentBase + CurrentTip) * 0.5f;

	/*
	 * 직전 고정 Animation Sample
	 * →
	 * 현재 고정 Animation Sample
	 */
	SweepSegment(PreviousBaseLocation, CurrentBase);
	SweepSegment(PreviousMiddle, CurrentMiddle);
	SweepSegment(PreviousTipLocation, CurrentTip);

	/*
	 * 현재 Sample 시점의
	 * 검날 전체 공간.
	 */
	SweepSegment(CurrentBase, CurrentTip);
	PreviousBaseLocation = CurrentBase;
	PreviousTipLocation = CurrentTip;
}

void UDRMeleeCombatComponent::SweepSegment(const FVector& Start, const FVector& End)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !IsValid(World))
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleeWeaponSweep), false, Character);
	QueryParams.AddIgnoredActor(Character);

	UDRMeleeWeaponItemDefinition* WeaponDefinition = ActiveWeaponDefinition.Get();

	if (!IsValid(WeaponDefinition))
	{
		return;
	}

	const float SweepRadius = WeaponDefinition->SweepRadius;
	
	TArray<FHitResult> HitResults;
	const bool bHit = World->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(SweepRadius), QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(World, Start, End, bHit ? FColor::Green : FColor::Red, false, 0.15f, 0, 2.f);
		DrawDebugSphere(World, End, SweepRadius, 12, bHit ? FColor::Green : FColor::Red, false, 0.15f);
	}
#endif

	if (!bHit)
	{
		return;
	}

	for (const FHitResult& HitResult : HitResults)
	{
		ADRPlayerCharacter* HitPlayer = Cast<ADRPlayerCharacter>(HitResult.GetActor());
		if (!IsValid(HitPlayer) || HitPlayer == Character)
		{
			continue;
		}

		ProcessHit(HitResult);
	}
}

bool UDRMeleeCombatComponent::EvaluateWeaponSweepSample(const UAnimMontage* Montage, const float SampleTime, FVector& OutBase, FVector& OutTip) const
{
	const ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !IsValid(Montage))
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	UStaticMeshComponent* WeaponMesh = Character->GetWorldHandEquipmentMesh();
	if (!IsValid(CharacterMesh) || !IsValid(WeaponMesh))
	{
		return false;
	}

	/*
	 * V4에서는 WeaponMesh의 현재 World Socket 위치를
	 * 절대 판정 기준으로 사용하지 않는다.
	 *
	 * GetSocketLocation() 사용 금지.
	 */

	/*
	 * Weapon이 실제로 Character SkeletalMesh에
	 * 붙어 있는지 검증한다.
	 */
	if (WeaponMesh->GetAttachParent() != CharacterMesh)
	{
		return false;
	}

	const FName AttachSocketName = WeaponMesh->GetAttachSocketName();

	if (AttachSocketName.IsNone())
	{
		return false;
	}

	USkeletalMesh* SkeletalMesh = CharacterMesh->GetSkeletalMeshAsset();

	UStaticMesh* StaticMesh = WeaponMesh->GetStaticMesh();

	if (!IsValid(SkeletalMesh) || !IsValid(StaticMesh))
	{
		return false;
	}

	/*
	 * ---------------------------------
	 * 1. Character Animation Pose 평가
	 * ---------------------------------
	 */

	const TSharedPtr<FBoneContainer> RequiredBones = CharacterMesh->GetSharedRequiredBones();

	if (!RequiredBones.IsValid())
	{
		return false;
	}

	/*
	 * ---------------------------------
	 * 1. Montage Time
	 *    → Source Animation Time
	 * ---------------------------------
	 */

	/*
	 * Montage 자체의 GetAnimationPose를 호출하지 않는다.
	 *
	 * Montage는 실제 공격 AnimSequence를 담고 있는
	 * Container / Timeline 역할로만 사용한다.
	 */
	const FAnimSegment* ActiveSegment = nullptr;

	for (const FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
	{
		ActiveSegment = SlotTrack.AnimTrack.GetSegmentAtTime(SampleTime);

		if (ActiveSegment != nullptr)
		{
			break;
		}
	}

	if (ActiveSegment == nullptr)
	{
		return false;
	}

	/*
	 * Montage 내부 Segment가 실제로 참조하는
	 * Animation Asset.
	 *
	 * 현재 AM_SwordAttack 안에 들어 있는
	 * 실제 Sword Attack Sequence가 여기 나온다.
	 */
	UAnimSequenceBase* SourceAnimation = ActiveSegment->GetAnimReference().Get();

	if (!IsValid(SourceAnimation))
	{
		return false;
	}

	/*
	 * Montage Track Time
	 * →
	 * Source Animation Time
	 *
	 * Segment의 PlayRate,
	 * Start/End 위치 등을 반영한다.
	 */
	const float SourceTime = ActiveSegment->ConvertTrackPosToAnimPos(SampleTime);
	
	/*
	 * ---------------------------------
	 * 2. Source Animation Pose 직접 평가
	 * ---------------------------------
	 */

	/*
	 * FCompactPose가 사용하는 stack allocation의
	 * lifetime을 현재 scope으로 제한한다.
	 */
	FMemMark MemMark(FMemStack::Get());

	FCompactPose LocalPose;
	LocalPose.SetBoneContainer(RequiredBones.Get());
	LocalPose.ResetToRefPose();

	FBlendedCurve Curve;
	Curve.InitFrom(*RequiredBones);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData PoseData(LocalPose, Curve, Attributes);
	const FAnimExtractContext ExtractionContext(static_cast<double>(SourceTime), false, FDeltaTimeRecord(), false);

	/*
	 * 핵심.
	 *
	 * Montage를 평가하는 것이 아니라
	 * Montage Segment가 참조하는 실제 Animation을
	 * 정확한 SourceTime에서 평가한다.
	 */
	SourceAnimation->GetAnimationPose(PoseData, ExtractionContext);

	/*
	 * Local Bone Pose
	 * →
	 * Component Space Pose
	 */
	FCSPose<FCompactPose> ComponentPose;

	ComponentPose.InitPose(LocalPose);

	/*
	 * ---------------------------------
	 * 2. Weapon Attachment Socket 찾기
	 * ---------------------------------
	 */

	FTransform AttachSocketLocal = FTransform::Identity;
	int32 AttachBoneMeshIndex = INDEX_NONE;
	int32 AttachSocketIndex = INDEX_NONE;
	USkeletalMeshSocket* AttachSocket = SkeletalMesh->FindSocketInfo(AttachSocketName, AttachSocketLocal, AttachBoneMeshIndex, AttachSocketIndex);

	if (AttachSocket == nullptr || AttachBoneMeshIndex == INDEX_NONE)
	{
		return false;
	}

	const FCompactPoseBoneIndex AttachBoneCompactIndex = RequiredBones->MakeCompactPoseIndex(FMeshPoseBoneIndex(AttachBoneMeshIndex));

	if (!AttachBoneCompactIndex.IsValid())
	{
		return false;
	}

	const FTransform& AttachBoneComponentTransform = ComponentPose.GetComponentSpaceTransform(AttachBoneCompactIndex);

	/*
	 * ---------------------------------
	 * 3. Weapon Base / Tip Local 위치
	 * ---------------------------------
	 */

	const UStaticMeshSocket* BaseSocket = StaticMesh->FindSocket(MeleeSweepBaseSocketName);
	const UStaticMeshSocket* TipSocket = StaticMesh->FindSocket(MeleeSweepTipSocketName);

	if (!IsValid(BaseSocket) || !IsValid(TipSocket))
	{
		return false;
	}

	const FVector BaseWeaponLocal = BaseSocket->RelativeLocation;
	const FVector TipWeaponLocal = TipSocket->RelativeLocation;

	/*
	 * WeaponMesh의 RelativeTransform은
	 * Attach Socket 기준 Weapon Transform이다.
	 *
	 * 현재 World Transform은 읽지 않는다.
	 */
	const FTransform WeaponRelativeTransform = WeaponMesh->GetRelativeTransform();
	const FTransform MeshWorldTransform = CharacterMesh->GetComponentTransform();

	/*
	 * ---------------------------------
	 * 4. Weapon Local
	 *      → Attach Socket
	 *      → Hand Bone
	 *      → Character Mesh
	 *      → World
	 * ---------------------------------
	 */

	auto WeaponPointToWorld = [&](const FVector& WeaponLocalPoint)
	{
		/*
		 * Weapon Local
		 * → Character Attach Socket Local
		 */
		const FVector PointInSocket = WeaponRelativeTransform.TransformPosition(WeaponLocalPoint);

		/*
		 * Attach Socket Local
		 * → Parent Hand Bone Local
		 */
		const FVector PointInBone = AttachSocketLocal.TransformPosition(PointInSocket);

		/*
		 * Bone Local
		 * → Character Mesh Component Space
		 */
		const FVector PointInMesh = AttachBoneComponentTransform.TransformPosition(PointInBone);

		/*
		 * Mesh Component Space
		 * → World Space
		 */
		return MeshWorldTransform.TransformPosition(PointInMesh);
	};

	OutBase = WeaponPointToWorld(BaseWeaponLocal);
	OutTip = WeaponPointToWorld(TipWeaponLocal);

	return true;
}

void UDRMeleeCombatComponent::ProcessHit(const FHitResult& HitResult)
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();
	ADRPlayerCharacter* HitPlayer = Cast<ADRPlayerCharacter>(HitResult.GetActor());
	if (!IsValid(Character) || !IsValid(HitPlayer) || HitPlayer == Character || HitPlayer->IsDead() || !bIsAttacking)
	{
		return;
	}

	if (AlreadyHitActors.Contains(HitPlayer))
	{
		return;
	}

	AlreadyHitActors.Add(HitPlayer);

	/*
	 * 여기서는 "맞았다"는 사실만 전달.
	 *
	 * 데미지/팀/빙결/구조/처형 정책은
	 * GameplayAbility가 결정한다.
	 */
	OnMeleeHitDetected.Broadcast(HitResult);
}

void UDRMeleeCombatComponent::CancelAttack()
{
	ADRPlayerCharacter* Character = GetOwnerCharacter();

	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return;
	}

	bHasPreviousSweepSample = false;
	bIsAttacking = false;

	AlreadyHitActors.Reset();
	ActiveWeaponDefinition.Reset();
}
