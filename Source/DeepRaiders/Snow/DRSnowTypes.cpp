#include "DRSnowTypes.h"

namespace
{
void SerializeFlag(FArchive& Ar, bool& Value)
{
	uint8 Bit = Value ? 1 : 0;
	Ar.SerializeBits(&Bit, 1);
	Value = Bit != 0;
}

void SerializePackedInt(FArchive& Ar, int32& Value)
{
	uint32 Encoded = (static_cast<uint32>(Value) << 1) ^ static_cast<uint32>(Value >> 31);
	Ar.SerializeIntPacked(Encoded);
	if (Ar.IsLoading())
	{
		Value = static_cast<int32>((Encoded >> 1) ^ -static_cast<int32>(Encoded & 1));
	}
}

void SerializeOptionalFloat(FArchive& Ar, float& Value, const float Default)
{
	bool bDifferent = FMemory::Memcmp(&Value, &Default, sizeof(float)) != 0;
	SerializeFlag(Ar, bDifferent);
	if (bDifferent)
	{
		Ar << Value;
	}
	else if (Ar.IsLoading())
	{
		Value = Default;
	}
}

template<typename T>
void SerializeEnum(FArchive& Ar, T& Value, const uint32 Count)
{
	uint32 Index = static_cast<uint32>(Value);
	if (Ar.IsSaving() && Index >= Count)
	{
		Ar.SetError();
		return;
	}
	Ar.SerializeInt(Index, Count);
	Value = static_cast<T>(Index);
}
}

bool FDRSnowOperationRecord::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (Ar.IsLoading())
	{
		// 같은 구조체 메모리를 재사용할 때 생략된 분기의 이전 값이 남지 않게 한다.
		*this = FDRSnowOperationRecord();
	}
	bool bSuccess = true;
	auto SerializeVector = [&](auto& Value)
	{
		bool bValueSuccess = true;
		Value.NetSerialize(Ar, Map, bValueSuccess);
		bSuccess &= bValueSuccess;
	};

	SerializePackedInt(Ar, Sequence);
	SerializeFlag(Ar, bIsAddOperation);
	if (bIsAddOperation)
	{
		FDRSnowAddOperation& Op = AddOperation;
		SerializeEnum(Ar, Op.EditTool, 4);
		SerializeVector(Op.WorldLocation);
		Ar << Op.VoxelWorldName;
		SerializePackedInt(Ar, Op.TeamId);
		Ar << Op.Amount;
		if (Op.EditTool == EDRSnowVoxelEditTool::OrientedBoxTool)
		{
			SerializeVector(Op.BoxExtent);
			SerializeVector(Op.BoxRotation);
		}
		else
		{
			Ar << Op.Radius;
		}
		if (Op.EditTool == EDRSnowVoxelEditTool::DirectionalSurfaceTool)
		{
			SerializeFlag(Ar, Op.bAllowVirtualSurfaceFallback);
			SerializeFlag(Ar, Op.bUseVirtualSurface);
			if (Op.bAllowVirtualSurfaceFallback || Op.bUseVirtualSurface)
			{
				SerializeVector(Op.SurfaceNormal);
			}
			if (Op.bUseVirtualSurface)
			{
				bool bHasSupportMask = Op.VirtualSurfaceSupportMask != 0;
				SerializeFlag(Ar, bHasSupportMask);
				if (bHasSupportMask)
				{
					Ar << Op.VirtualSurfaceSupportMask;
				}
				else if (Ar.IsLoading())
				{
					Op.VirtualSurfaceSupportMask = 0;
				}
			}
			SerializeOptionalFloat(Ar, ServerAppliedAmount, Op.Amount);
		}
		else if (Ar.IsLoading())
		{
			ServerAppliedAmount = Op.Amount;
		}
		// ImpactDirection은 현재 복셀/Volume 재생에서 읽지 않는다.
		// SurfaceNormal도 방향성 가상 표면 생성에서만 필요하다.
	}
	else
	{
		FDRSnowRemoveOperation& Op = RemoveOperation;
		SerializeEnum(Ar, Op.RemovalMode, 3);
		SerializeVector(Op.WorldLocation);
		Ar << Op.VoxelWorldName;
		SerializePackedInt(Ar, Op.TeamId);
		Ar << Op.Radius;
		Ar << Op.RequestedAmount;
		SerializeOptionalFloat(Ar, Op.AppliedAmount, Op.RequestedAmount);
		if (Op.RemovalMode != EDRSnowRemovalMode::InstantVolume)
		{
			SerializeVector(Op.BrushOrigin);
		}
		if (Op.RemovalMode == EDRSnowRemovalMode::ContactBrush)
		{
			SerializeVector(Op.SurfaceNormal);
		}
		if (Op.RemovalMode == EDRSnowRemovalMode::AbsorbTool)
		{
			SerializeOptionalFloat(Ar, Op.AbsorbInnerRadiusRatio, 0.45f);
			SerializeFlag(Ar, Op.bUseAdaptiveAbsorbQuery);
			if (Op.bUseAdaptiveAbsorbQuery)
			{
				SerializeOptionalFloat(Ar, Op.AbsorbSweepRadius, 50.f);
				SerializePackedInt(Ar, Op.AbsorbMaxSweepsPerTick);
			}
		}
		else
		{
			SerializeEnum(Ar, Op.RemovalBrushShape, 2);
		}
	}
	bOutSuccess = bSuccess && !Ar.IsError();
	return bOutSuccess;
}
