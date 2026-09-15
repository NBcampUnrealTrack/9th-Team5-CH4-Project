#pragma once

#include "Engine/EngineTypes.h"

namespace DRCollisionChannels
{
	inline constexpr ECollisionChannel Projectile = ECC_GameTraceChannel1;
	inline constexpr ECollisionChannel Interaction = ECC_GameTraceChannel2;
	inline constexpr ECollisionChannel Grapple = ECC_GameTraceChannel3;
	/** 물리 투사체는 무시하고 히트스캔 Query만 배리어에 적중시키는 Object Channel이다. */
	inline constexpr ECollisionChannel BarrierTrace = ECC_GameTraceChannel4;
	inline constexpr ECollisionChannel Breakable = ECC_GameTraceChannel5;
}
