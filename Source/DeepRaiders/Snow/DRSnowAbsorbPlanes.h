#pragma once

#include "DRSnowTypes.h"

namespace Chaos { class FImplicitObject; }
namespace DRSnowAbsorbPlanes
{
bool BuildVolume(const Chaos::FImplicitObject& Geometry, const FMatrix& ToWorld,
	const FVector& Origin, FDRSnowAbsorbConvex& OutVolume);
// 원자적으로 처리한다. 실패 시 아무것도 추가하지 않으며, 호출자는 해당
// 컴포넌트/인스턴스의 보수적 물리 차폐를 그대로 유지한다.
bool TrySnapshot(FBodyInstance& Body, const FVector& Origin,
	TArray<FDRSnowAbsorbConvex>& OutVolumes);

inline bool IsBlocked(const FDRSnowAbsorbConvex& Volume, const FVector& PointDelta)
{
	if (Volume.Planes.Num() < 4) { return true; }
	double Enter = 0., Exit = 1.;
	double EntryPointDistance = 0.;
	bool bOriginInside = true, bPointInside = true;
	for (const FVector4& P : Volume.Planes)
	{
		const double Start = -P.W;
		const double End = P.X * PointDelta.X + P.Y * PointDelta.Y + P.Z * PointDelta.Z - P.W;
		bOriginInside &= Start <= 0.;
		bPointInside &= End <= 0.;
		if (Start > 0. && End > 0.) { return false; }
		const double Change = End - Start;
		if (FMath::Abs(Change) < 1.e-12) { continue; }
		const double T = -Start / Change;
		if (Change < 0. && T > Enter) { Enter = T; EntryPointDistance = End; }
		if (Change > 0.) { Exit = FMath::Min(Exit, T); }
		if (Enter > Exit) { return false; }
	}
	if (bOriginInside) { return true; }
	// 매우 얇은 메쉬라도 출구를 지난 지점은 계속 차폐한다.
	// 진입면에서 1cm 이내이면서 아직 내부에 있는 지점만 편집을 허용한다.
	return !(bPointInside && -EntryPointDistance <= Volume.InsetCm);
}

inline bool IsBlocked(const TArray<FDRSnowAbsorbConvex>& Volumes, const FVector& PointDelta)
{
	for (const FDRSnowAbsorbConvex& Volume : Volumes)
	{
		if (IsBlocked(Volume, PointDelta)) { return true; }
	}
	return false;
}
}
