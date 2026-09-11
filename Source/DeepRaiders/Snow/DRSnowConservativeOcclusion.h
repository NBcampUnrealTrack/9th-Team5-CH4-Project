#pragma once

#include "CoreMinimal.h"
#include "Chaos/Convex.h"
#include "DRSnowTypes.h"

// 접두 부피는 타일 중앙 레이만이 아니라 타일 안의 모든 경로를 감싼다. 모든 Bounds는
// 보수적으로 사용한다. 오탐은 눈을 남길 수 있지만 미탐으로 차폐를 통과해서는 안 된다.
namespace DRSnowConservativeOcclusion
{
struct FBuilder
{
	FVector Origin;
	FVector Direction;
	FVector AxisY;
	FVector AxisZ;
	float Range = 0.f;
	float InnerRadius = 0.f;
	float OuterRadius = 0.f;
	TArray<FBox> Obstacles;
	// 월드 AABB는 넓은 단계에서만 사용한다. 좁은 단계에는 복셀 크기 Padding 없이
	// Origin 상대 좌표로 만든 실제 타일 Hull을 전달한다.
	TFunction<bool(int32, const Chaos::FConvex&)> IntersectsGeometry;
	int32 RemainingTests = 8192;
	int32 Tests = 0;
	bool bBudgetExceeded = false;
	int32 HullsBuilt = 0;
	int32 InvalidHulls = 0;
	TArray<uint8> Depths;

	TArray<Chaos::FConvex::FVec3Type> PrefixVertices(int32 X, int32 Y, int32 Size, float Depth) const
	{
		TArray<Chaos::FConvex::FVec3Type> Vertices;
		Vertices.Reserve(9);
		// 중앙 Origin도 포함한다. 흡수구와 0이 아닌 시작 단면 사이의 장애물을
		// 흡수구 중심 경로가 우회해서는 안 된다.
		Vertices.Add(Chaos::FConvex::FVec3Type(0.f));
		for (int32 End = 0; End < 2; ++End)
		{
			const float D = End == 0 ? 0.f : Depth;
			const float Radius = FMath::Lerp(InnerRadius, OuterRadius, D / Range);
			for (int32 U = 0; U < 2; ++U)
			for (int32 V = 0; V < 2; ++V)
			{
				const float NX = 2.f * (X + U * Size) / DRSnowAbsorbOcclusion::Resolution - 1.f;
				const float NY = 2.f * (Y + V * Size) / DRSnowAbsorbOcclusion::Resolution - 1.f;
				Vertices.Add(Chaos::FConvex::FVec3Type(
					Direction * D + AxisY * (NX * Radius) + AxisZ * (NY * Radius)));
			}
		}
		return Vertices;
	}

	FBox PrefixBounds(int32 X, int32 Y, int32 Size, float Depth) const
	{
		FBox Bounds(ForceInit);
		for (const auto& Vertex : PrefixVertices(X, Y, Size, Depth))
		{
			Bounds += Origin + FVector(Vertex);
		}
		return Bounds;
	}

	TUniquePtr<Chaos::FConvex> MakePrefixGeometry(int32 X, int32 Y, int32 Size, float Depth) const
	{
		TArray<Chaos::FConvex::FPlaneType> Planes;
		TArray<TArray<int32>> Faces;
		TArray<Chaos::FConvex::FVec3Type> Vertices;
		Chaos::FConvex::FAABB3Type Bounds;
		// 편의 생성자의 1cm 면 단순화는 사용하지 않는다. 메쉬 앞면 가까이의
		// 매우 짧은 접두 부피를 변형할 수 있기 때문이다.
		Chaos::FConvexBuilder::Build(PrefixVertices(X, Y, Size, Depth),
			Planes, Faces, Vertices, Bounds, Chaos::FConvexBuilder::EBuildMethod::ConvexHull3);
		if (Planes.Num() < 4 || Vertices.Num() < 4 || Planes.Num() != Faces.Num())
		{
			return nullptr;
		}
		return MakeUnique<Chaos::FConvex>(MoveTemp(Planes), MoveTemp(Faces), MoveTemp(Vertices));
	}

	bool IsClear(int32 X, int32 Y, int32 Size, float Depth)
	{
		const FBox Bounds = PrefixBounds(X, Y, Size, Depth);
		TUniquePtr<Chaos::FConvex> Geometry;
		for (int32 Index = 0; Index < Obstacles.Num(); ++Index)
		{
			if (RemainingTests <= 0)
			{
				bBudgetExceeded = true;
				return false;
			}
			--RemainingTests;
			++Tests;
			if (!Bounds.Intersect(Obstacles[Index])) { continue; }
			// 차폐 형상을 AABB로 조용히 대체하지 않는다.
			if (!IntersectsGeometry) { return false; }
			if (!Geometry)
			{
				Geometry = MakePrefixGeometry(X, Y, Size, Depth);
				++HullsBuilt;
				if (!Geometry || !Geometry->IsValidGeometry())
				{
					++InvalidHulls;
					return false;
				}
			}
			if (IntersectsGeometry(Index, *Geometry)) { return false; }
		}
		return true;
	}

	void Visit(int32 X, int32 Y, int32 Size)
	{
		if (RemainingTests <= 0) { bBudgetExceeded = true; return; }
		if (IsClear(X, Y, Size, Range))
		{
			for (int32 V = Y; V < Y + Size; ++V)
			for (int32 U = X; U < X + Size; ++U)
			{
				Depths[DRSnowAbsorbOcclusion::GetCellIndex(U, V)] = DRSnowAbsorbOcclusion::OpenDepth;
			}
			return;
		}
		if (Size > 1)
		{
			const int32 Half = Size / 2;
			Visit(X, Y, Half); Visit(X + Half, Y, Half);
			Visit(X, Y + Half, Half); Visit(X + Half, Y + Half, Half);
			return;
		}
		// 정수 깊이를 직접 탐색한다. 접두 부피 전체가 비어 있음이 검증된 경우에만
		// Low를 늘리며, 예산 소진으로 미검사 구간이 열려서는 안 된다.
		int32 Low = 0;
		int32 High = DRSnowAbsorbOcclusion::MaxBlockedDepth;
		while (Low < High && RemainingTests > 0)
		{
			const int32 Mid = (Low + High + 1) / 2;
			if (IsClear(X, Y, 1, Range * Mid / DRSnowAbsorbOcclusion::MaxBlockedDepth)) { Low = Mid; }
			else { High = Mid - 1; }
		}
		Depths[DRSnowAbsorbOcclusion::GetCellIndex(X, Y)] = static_cast<uint8>(Low);
	}

	TArray<uint8> Build()
	{
		Depths.Init(0, DRSnowAbsorbOcclusion::SampleCount);
		if (Range > KINDA_SMALL_NUMBER) { Visit(0, 0, DRSnowAbsorbOcclusion::Resolution); }
		return MoveTemp(Depths);
	}
};
}
