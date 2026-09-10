#pragma once

#include "CoreMinimal.h"
#include "VoxelWorld.h"
#include "DeepRaiders/Snow/Deposit/DRVoxelDepositOperations.h"
#include "DRSnowTypes.generated.h"

// TeamId는 게임 규칙용 식별자이고 MaterialIndex는 Voxel 표현용 식별자다.
// 현재 프로젝트의 고정 규칙(Neutral=0, Team N=N+1)을 한곳에서 관리한다.
namespace DRSnowMaterialMapping
{
	FORCEINLINE uint8 TeamToMaterialIndex(const int32 TeamId)
	{
		if (TeamId == INDEX_NONE)
		{
			return 0;
		}

		ensureMsgf(
			TeamId >= 0 && TeamId <= MAX_uint8 - 1,
			TEXT("Snow TeamId %d cannot be represented by a uint8 MaterialIndex"),
			TeamId);
		return static_cast<uint8>(
			FMath::Clamp(TeamId, 0, static_cast<int32>(MAX_uint8) - 1) + 1);
	}

	FORCEINLINE int32 MaterialIndexToTeamId(const uint8 MaterialIndex)
	{
		return MaterialIndex == 0
			? INDEX_NONE
			: static_cast<int32>(MaterialIndex) - 1;
	}
}

// StaticMesh virtual-surface hits use a tiny fixed 7x7 support mask.
// The server builds it from the actual hit component and replicates the 49 bits
// so client replay clips the same virtual footprint without doing local traces.
namespace DRSnowVirtualSurfaceSupport
{
	static constexpr int32 Resolution = 7;
	static constexpr int32 SampleCount = Resolution * Resolution;
	static_assert(SampleCount <= 63, "Virtual surface support mask must fit in signed int64");

	FORCEINLINE int32 GetBitIndex(const int32 X, const int32 Y)
	{
		return Y * Resolution + X;
	}

	FORCEINLINE void BuildBasis(const FVector& InNormal, FVector& OutTangent, FVector& OutBitangent)
	{
		const FVector Normal = InNormal.GetSafeNormal();
		const FVector Reference = FMath::Abs(Normal.Z) < 0.99f
			? FVector::UpVector
			: FVector::ForwardVector;
		OutTangent = FVector::CrossProduct(Reference, Normal).GetSafeNormal();
		OutBitangent = FVector::CrossProduct(Normal, OutTangent).GetSafeNormal();
	}
}


class AActor;
class APawn;

UENUM(BlueprintType)
enum class EDRSnowVoxelEditTool : uint8
{
	// Voxel Plugin의 surface voxel 탐색 결과를 기준으로 표면을 따라 값을 조정한다.
	SurfaceTool UMETA(DisplayName = "Surface Tool"),

	// 지정 반경의 구 부피를 직접 더하거나 뺀다. SurfaceTool과 제거 느낌을 비교할 때 사용한다.
	SphereTool UMETA(DisplayName = "Sphere Tool"),

	// surface footprint만 표면에서 찾고, 실제 값 변경은 요청 방향으로만 적용한다.
	DirectionalSurfaceTool UMETA(DisplayName = "Directional Surface Tool"),

	// 월드 공간에서 회전 가능한 직육면체 부피를 직접 채운다. 설치형 눈벽 전용이다.
	OrientedBoxTool UMETA(DisplayName = "Oriented Box Tool"),

};

UENUM(BlueprintType)
enum class EDRSnowRemovalBrushShape : uint8
{
	Sphere UMETA(DisplayName = "Sphere"),
	Box UMETA(DisplayName = "Box")
};

UENUM(BlueprintType)
enum class EDRSnowRemovalMode : uint8
{
	// 청소기처럼 brush를 표면 바깥에서 얕게 관통시킨다.
	ContactBrush UMETA(DisplayName = "Contact Brush"),

	// 폭탄처럼 히트 지점을 중심으로 shape 전체를 한 번에 제거한다.
	InstantVolume UMETA(DisplayName = "Instant Volume"),

	// 흡수구에서 멀어질수록 약해지는 frustum 범위로 표면 눈을 조금씩 제거한다.
	AbsorbTool UMETA(DisplayName = "Absorb Tool")
};

// 눈 관련 요청을 누가 발생시켰는지 기록한다.
// 팀 판정은 별도 enum을 만들지 않고 PlayerState의 TeamId 체계를 그대로 따른다.
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowInteractionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	TObjectPtr<APawn> InstigatorPawn = nullptr;
};

// 표면에 눈을 쌓을 때 사용하는 공통 요청 데이터다.
// 실제 Voxel 편집은 중앙 시스템에서 처리한다.
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowSurfaceAddRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FVector SurfaceNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FVector ImpactDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	TObjectPtr<AVoxelWorld> TargetVoxelWorld = nullptr;

	// SnowVolumeSubsystem에는 같은 반경으로 팀별 density를 기록하고,
	// SnowSurfaceSubsystem에는 같은 반경으로 Voxel 표면 표현을 만든다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0"))
	float Amount = 1.f;

	/** OrientedBoxTool일 때 사용할 로컬 반쪽 크기다. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0", Units = "cm"))
	FVector BoxExtent = FVector::ZeroVector;

	/** OrientedBoxTool일 때 BoxExtent가 따르는 월드 회전이다. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FRotator BoxRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	EDRSnowVoxelEditTool EditTool = EDRSnowVoxelEditTool::SurfaceTool;

	// DirectionalSurfaceTool이 기존 Voxel 표면을 찾지 못했을 때
	// HitResult의 위치/노멀을 가상 표면으로 사용해 허공 복셀을 생성할지 여부다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	bool bAllowVirtualSurfaceFallback = false;

	// VoxelWorld가 아닌 Hit 표면을 직접 기준으로 삼아 DirectionalSurfaceTool footprint를 만든다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	bool bUseVirtualSurface = false;

	// 7x7 StaticMesh support samples. 0 means legacy/unmasked virtual plane.
	// UPROPERTY also carries the value through the detailed ServerTryAddSnow(Request) RPC.
	UPROPERTY()
	int64 VirtualSurfaceSupportMask = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FDRSnowInteractionContext Context;
};

// 표면의 눈을 흡수/제거할 때 사용하는 공통 요청 데이터다.
// RequestedAmount는 청소기 업그레이드로 조정되는 1회 흡수 강도이며,
// 최종 탄약 회복량은 실제 제거량 반환값을 기준으로 계산해야 한다.
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowSurfaceRemoveRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FVector SurfaceNormal = FVector::UpVector;

	// 제거 brush가 날아오는 시작점이다. 접촉형 제거는 이 위치에서 히트 지점으로 향한다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FVector BrushOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	TObjectPtr<AVoxelWorld> TargetVoxelWorld = nullptr;

	// 흡수/제거가 영향을 주는 월드 반경이다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 100.f;

	// 1회 제거 강도이며, SnowVolume 감소량의 기준이 된다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0"))
	float RequestedAmount = 1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	EDRSnowRemovalBrushShape RemovalBrushShape = EDRSnowRemovalBrushShape::Sphere;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	EDRSnowRemovalMode RemovalMode = EDRSnowRemovalMode::ContactBrush;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AbsorbInnerRadiusRatio = 0.45f;

	// Adaptive 거리 필드 slab의 최소 절반 깊이다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "1.0", Units = "cm"))
	float AbsorbSweepRadius = 50.f;

	// 한 흡수 틱에 허용할 최대 거리 필드 slab 조회 수다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "1"))
	int32 AbsorbMaxSweepsPerTick = 32;

	// 무기 설정에서 선택한 Absorb Tool surface query 방식이다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	bool bUseAdaptiveAbsorbQuery = true;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FDRSnowInteractionContext Context;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowAddOperation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FVector_NetQuantize WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FVector_NetQuantizeNormal SurfaceNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FVector_NetQuantizeNormal ImpactDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	float Radius = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	float Amount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FVector_NetQuantize BoxExtent = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FRotator BoxRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	EDRSnowVoxelEditTool EditTool = EDRSnowVoxelEditTool::SurfaceTool;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	bool bAllowVirtualSurfaceFallback = false;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	bool bUseVirtualSurface = false;

	// Replicated by FDRSnowOperationRecord::NetSerialize only for virtual surfaces.
	UPROPERTY()
	int64 VirtualSurfaceSupportMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FName VoxelWorldName = NAME_None;
};

USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowRemoveOperation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FVector_NetQuantize WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FVector_NetQuantizeNormal SurfaceNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FVector_NetQuantize BrushOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	float Radius = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	float RequestedAmount = 0.f;

	// 서버 표면 편집에서 실제로 빠진 양이다. 클라이언트는 SnowVolume 감소에
	// 이 값을 사용해야 각자의 표면 탐색 결과 차이로 원본 density가 벌어지지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	float AppliedAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	EDRSnowRemovalBrushShape RemovalBrushShape = EDRSnowRemovalBrushShape::Sphere;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	EDRSnowRemovalMode RemovalMode = EDRSnowRemovalMode::ContactBrush;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AbsorbInnerRadiusRatio = 0.45f;

	// Adaptive 거리 필드 slab의 최소 절반 깊이다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network", meta = (ClampMin = "1.0", Units = "cm"))
	float AbsorbSweepRadius = 50.f;

	// 한 흡수 틱에 허용할 최대 거리 필드 slab 조회 수다.
	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network", meta = (ClampMin = "1"))
	int32 AbsorbMaxSweepsPerTick = 32;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	bool bUseAdaptiveAbsorbQuery = true;


	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FName VoxelWorldName = NAME_None;
};

// checkpoint 이후 재생할 눈 변경 이벤트다. Sequence는 중도난입 동기화 중
// multicast와 history가 겹쳐도 같은 변경을 한 번만 적용하기 위한 기준이다.
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowOperationRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	bool bIsAddOperation = true;

	UPROPERTY()
	bool bIsDepositOperation = false;

	UPROPERTY()
	FDRVoxelDepositResult DepositOperation;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FDRSnowAddOperation AddOperation;

	UPROPERTY(BlueprintReadOnly, Category = "Snow|Network")
	FDRSnowRemoveOperation RemoveOperation;

	// 방향성 눈 추가에서 서버가 실제로 적용한 양
	UPROPERTY()
	float ServerAppliedAmount = 0.f;

	// 작업 종류와 도구에 필요한 필드만 전송한다. 위치/float 정밀도는 기존과 동일하다.
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FDRSnowOperationRecord> : TStructOpsTypeTraitsBase2<FDRSnowOperationRecord>
{
	enum { WithNetSerializer = true };
};

// 눈 투사체나 눈 충돌체가 캐릭터/대상에게 피해를 줄 때 사용하는 요청 데이터다.
// 실제 체력 감소는 이후 GAS GameplayEffect 적용 계층에서 처리한다.
USTRUCT(BlueprintType)
struct DEEPRAIDERS_API FDRSnowDamageRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FVector HitNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0"))
	float DamageAmount = 1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FDRSnowInteractionContext Context;
};
