#include "DRSnowAbsorbPlanes.h"

#include "Chaos/Box.h"
#include "Chaos/Convex.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/CollisionFilterData.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Math/ScaleMatrix.h"
#include "Physics/Experimental/PhysInterface_Chaos.h"
#include "PhysicsEngine/BodyInstance.h"

namespace DRSnowAbsorbPlanes
{
namespace
{
// 다면체 충돌 Hull은 수학적으로 볼록하더라도, 실제 메시의 외곽을 크게 감싼
// 단순 충돌인 경우가 많다. 흡수 표면 판정에는 상자/저면 수가 적은 Hull만
// 해석적으로 사용하고, 그보다 복잡한 Hull은 컴포넌트 첫 표면 Trace 경로로 보낸다.
constexpr int32 MaxAnalyticAbsorbConvexPlanes = 12;

bool ReadPolytope(const Chaos::FImplicitObject& Geometry, const FMatrix& ToWorld,
	TArray<FPlane>& OutPlanes, TArray<FVector>& OutVertices, int32 Depth = 0)
{
	if (Depth > 8) { return false; }
	using namespace Chaos;
	switch (Geometry.GetType())
	{
	case ImplicitObjectType::Transformed:
	{
		const auto& T = Geometry.GetObjectChecked<TImplicitObjectTransformed<FReal, 3>>();
		return ReadPolytope(*T.GetTransformedObject(), FTransform(T.GetTransform()).ToMatrixWithScale() * ToWorld,
			OutPlanes, OutVertices, Depth + 1);
	}
	case ImplicitObjectType::IsScaled | ImplicitObjectType::Convex:
	{
		const auto& S = Geometry.GetObjectChecked<TImplicitObjectScaled<FConvex>>();
		return ReadPolytope(*S.GetUnscaledObject(), FScaleMatrix(FVector(S.GetScale())) * ToWorld,
			OutPlanes, OutVertices, Depth + 1);
	}
	case ImplicitObjectType::IsScaled | ImplicitObjectType::Box:
	{
		const auto& S = Geometry.GetObjectChecked<TImplicitObjectScaled<TBox<FReal, 3>>>();
		return ReadPolytope(*S.GetUnscaledObject(), FScaleMatrix(FVector(S.GetScale())) * ToWorld,
			OutPlanes, OutVertices, Depth + 1);
	}
	case ImplicitObjectType::IsInstanced | ImplicitObjectType::Convex:
		return ReadPolytope(*Geometry.GetObjectChecked<TImplicitObjectInstanced<FConvex>>().GetInstancedObject(),
			ToWorld, OutPlanes, OutVertices, Depth + 1);
	case ImplicitObjectType::IsInstanced | ImplicitObjectType::Box:
		return ReadPolytope(*Geometry.GetObjectChecked<TImplicitObjectInstanced<TBox<FReal, 3>>>().GetInstancedObject(),
			ToWorld, OutPlanes, OutVertices, Depth + 1);
	case ImplicitObjectType::Box:
	{
		const auto& Box = Geometry.GetObjectChecked<TBox<FReal, 3>>();
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			FVector N(0.); N[Axis] = 1.;
			OutPlanes.Add(FPlane(FVector(Box.Max()), N).TransformBy(ToWorld));
			OutPlanes.Add(FPlane(FVector(Box.Min()), -N).TransformBy(ToWorld));
		}
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			FVector P;
			for (int32 Axis = 0; Axis < 3; ++Axis) { P[Axis] = (Corner & (1 << Axis)) ? Box.Max()[Axis] : Box.Min()[Axis]; }
			OutVertices.Add(FVector(ToWorld.TransformPosition(P)));
		}
		return true;
	}
	case ImplicitObjectType::Convex:
	{
		const auto& Convex = Geometry.GetObjectChecked<FConvex>();
		if (Convex.NumPlanes() > MaxAnalyticAbsorbConvexPlanes || Convex.NumVertices() > 256) { return false; }
		for (int32 I = 0; I < Convex.NumPlanes(); ++I)
		{
			const auto P = Convex.GetPlane(I);
			OutPlanes.Add(FPlane(FVector(P.X()), FVector(P.Normal())).TransformBy(ToWorld));
		}
		for (int32 I = 0; I < Convex.NumVertices(); ++I)
		{
			OutVertices.Add(FVector(ToWorld.TransformPosition(FVector(Convex.GetVertex(I)))));
		}
		return true;
	}
	default: return false; // 곡면/삼각형/미확인 형상은 물리 차폐 대체 경로를 유지한다.
	}
}

bool Canonicalize(const TArray<FPlane>& Planes, const TArray<FVector>& Vertices,
	const FVector& Origin, FDRSnowAbsorbConvex& Out)
{
	if (Planes.Num() < 4 || Vertices.Num() < 4) { return false; }
	double MinWidth = TNumericLimits<double>::Max();
	for (FPlane P : Planes)
	{
		if (!P.Normalize() || !FMath::IsFinite(P.W)) { return false; }
		const FVector N(P.X, P.Y, P.Z);
		double Width = 0.;
		for (const FVector& V : Vertices)
		{
			if (V.ContainsNaN()) { return false; }
			const double SignedDistance = P.PlaneDot(V);
			if (SignedDistance > 0.05) { return false; } // 면 방향이 잘못되었거나 유효하지 않은 Hull이다.
			Width = FMath::Max(Width, -SignedDistance);
		}
		MinWidth = FMath::Min(MinWidth, Width);
		const FVector4 Packed(static_cast<float>(P.X), static_cast<float>(P.Y), static_cast<float>(P.Z),
			static_cast<float>(P.W - FVector::DotProduct(N, Origin)));
		if (!FMath::IsFinite(Packed.X) || !FMath::IsFinite(Packed.Y) ||
			!FMath::IsFinite(Packed.Z) || !FMath::IsFinite(Packed.W))
		{
			return false;
		}
		bool bDuplicate = false;
		for (const FVector4& Existing : Out.Planes)
		{
			if (FMath::Abs(Existing.X - Packed.X) < 1.e-6 && FMath::Abs(Existing.Y - Packed.Y) < 1.e-6 &&
				FMath::Abs(Existing.Z - Packed.Z) < 1.e-6 && FMath::Abs(Existing.W - Packed.W) < 0.001)
			{ bDuplicate = true; break; }
		}
		if (!bDuplicate) { Out.Planes.Add(Packed); }
	}
	Out.InsetCm = static_cast<float>(FMath::Clamp(
		MinWidth * 0.25,
		0.,
		static_cast<double>(SurfaceAbsorbAllowanceCm)));
	return Out.Planes.Num() >= 4 && Out.Planes.Num() <= FDRSnowAbsorbConvex::MaxPlanes && MinWidth > 0.;
}
}

bool BuildVolume(const Chaos::FImplicitObject& Geometry, const FMatrix& ToWorld,
	const FVector& Origin, FDRSnowAbsorbConvex& OutVolume)
{
	TArray<FPlane> Planes;
	TArray<FVector> Vertices;
	FDRSnowAbsorbConvex Pending;
	if (!ReadPolytope(Geometry, ToWorld, Planes, Vertices) || !Canonicalize(Planes, Vertices, Origin, Pending))
	{ return false; }
	OutVolume = MoveTemp(Pending);
	return true;
}

bool TrySnapshot(FBodyInstance& Body, const FVector& Origin, TArray<FDRSnowAbsorbConvex>& OutVolumes)
{
	if (!Body.IsValidBodyInstance() || Body.WeldParent) { return false; }
	TArray<FDRSnowAbsorbConvex> Pending;
	bool bValid = true;
	const bool bRead = FPhysicsCommand::ExecuteRead(Body.GetPhysicsActor(), [&](const FPhysicsActorHandle& Actor)
	{
		TArray<FPhysicsShapeHandle> Shapes;
		FPhysicsInterface::GetAllShapes_AssumedLocked(Actor, Shapes);
		const FMatrix ToWorld = FPhysicsInterface::GetGlobalPose_AssumesLocked(Actor).ToMatrixWithScale();
		bool bHasSimple = false;
		for (const FPhysicsShapeHandle& Shape : Shapes)
		{
			if (!FPhysicsInterface::IsQueryShape(Shape) || !Body.IsShapeBoundToBody(Shape)) { continue; }
			bHasSimple |= FPhysicsInterface::GetCombinedShapeFilterData(Shape).GetShapeFilterData().HasFlag(Chaos::EFilterFlags::SimpleCollision);
		}
		// Simple Query Collision이 있으면 우선한다. 없을 때만 Complex Query
		// Collision을 사용한다. 선택된 형상 하나라도 표현하지 못하면 해당 Body
		// 전체를 대체 차폐 경로로 보내어 부분적으로 열린 경로가 생기지 않게 한다.
		for (const FPhysicsShapeHandle& Shape : Shapes)
		{
			if (!FPhysicsInterface::IsQueryShape(Shape) || !Body.IsShapeBoundToBody(Shape)) { continue; }
			const auto Filter = FPhysicsInterface::GetCombinedShapeFilterData(Shape);
			if (!Filter.GetShapeFilterData().HasFlag(bHasSimple ? Chaos::EFilterFlags::SimpleCollision : Chaos::EFilterFlags::ComplexCollision)) { continue; }
			FDRSnowAbsorbConvex Volume;
			if (!BuildVolume(Shape.GetGeometry(), ToWorld, Origin, Volume))
			{
				bValid = false;
				break;
			}
			Pending.Add(MoveTemp(Volume));
			if (Pending.Num() + OutVolumes.Num() > FDRSnowAbsorbConvex::MaxVolumes) { bValid = false; break; }
		}
	});
	int32 TotalPlanes = 0;
	for (const auto& V : OutVolumes) { TotalPlanes += V.Planes.Num(); }
	for (const auto& V : Pending) { TotalPlanes += V.Planes.Num(); }
	if (!bRead || !bValid || Pending.IsEmpty() || TotalPlanes > FDRSnowAbsorbConvex::MaxTotalPlanes) { return false; }
	OutVolumes.Append(MoveTemp(Pending));
	return true;
}
}
