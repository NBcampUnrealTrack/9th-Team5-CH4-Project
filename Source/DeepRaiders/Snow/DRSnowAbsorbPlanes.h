#pragma once

#include "DRSnowTypes.h"

namespace Chaos { class FImplicitObject; }
namespace DRSnowAbsorbPlanes
{
// StaticMesh 표면에 붙은 눈이 남지 않도록, 표면 안쪽으로 허용할 최대 흡수 여유다.
inline constexpr float SurfaceAbsorbAllowanceCm = 1.f;

bool BuildVolume(const Chaos::FImplicitObject& Geometry, const FMatrix& ToWorld,
	const FVector& Origin, FDRSnowAbsorbConvex& OutVolume);
// 원자적으로 처리한다. 실패 시 아무것도 추가하지 않으며, 호출자는 해당
// 컴포넌트/인스턴스의 보수적 물리 차폐를 그대로 유지한다.
bool TrySnapshot(FBodyInstance& Body, const FVector& Origin,
	TArray<FDRSnowAbsorbConvex>& OutVolumes);

inline bool IsBlocked(
	const FDRSnowAbsorbConvex& Volume,
	const FVector& PointDelta,
	const float MinimumSurfaceAllowanceCm = 0.f)
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
	// 진입면의 허용 범위 이내이면서 아직 내부에 있는 지점만 편집을 허용한다.
	const float SurfaceAllowanceCm = FMath::Max(Volume.InsetCm, MinimumSurfaceAllowanceCm);
	return !(bPointInside && -EntryPointDistance <= SurfaceAllowanceCm);
}

// 동일한 광선에 여러 차폐 볼륨이 겹치면, 플레이어 쪽에서 먼저 만나는
// 표면만 유효한 차폐면이다. 뒤쪽 볼륨까지 OR로 적용하면 앞 표면의 Inset
// 허용 영역이 즉시 다시 막혀 표면에 붙은 눈이 남는다.
inline bool TryGetEntryFraction(const FDRSnowAbsorbConvex& Volume, const FVector& PointDelta, double& OutEnter)
{
	if (Volume.Planes.Num() < 4) { return false; }
	double Enter = 0., Exit = 1.;
	for (const FVector4& P : Volume.Planes)
	{
		const double Start = -P.W;
		const double End = P.X * PointDelta.X + P.Y * PointDelta.Y + P.Z * PointDelta.Z - P.W;
		if (Start > 0. && End > 0.) { return false; }
		const double Change = End - Start;
		if (FMath::Abs(Change) < 1.e-12) { continue; }
		const double T = -Start / Change;
		if (Change < 0.) { Enter = FMath::Max(Enter, T); }
		else { Exit = FMath::Min(Exit, T); }
		if (Enter > Exit) { return false; }
	}
	OutEnter = Enter;
	return true;
}

inline bool IsBlocked(
	const TArray<FDRSnowAbsorbConvex>& Volumes,
	const FVector& PointDelta,
	const float MinimumSurfaceAllowanceCm = 0.f)
{
	const FDRSnowAbsorbConvex* NearestVolume = nullptr;
	double NearestEnter = TNumericLimits<double>::Max();
	for (const FDRSnowAbsorbConvex& Volume : Volumes)
	{
		double Enter = 0.;
		if (TryGetEntryFraction(Volume, PointDelta, Enter) && Enter < NearestEnter)
		{
			NearestEnter = Enter;
			NearestVolume = &Volume;
		}
	}
	return NearestVolume && IsBlocked(*NearestVolume, PointDelta, MinimumSurfaceAllowanceCm);
}
}
