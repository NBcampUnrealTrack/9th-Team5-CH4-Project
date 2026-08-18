#pragma once

#include "CoreMinimal.h"
#include "VoxelWorld.h"
#include "DRSnowTypes.generated.h"

class AActor;
class APawn;

UENUM(BlueprintType)
enum class EDRSnowVoxelEditTool : uint8
{
	// Voxel Plugin의 surface voxel 탐색 결과를 기준으로 표면을 따라 값을 조정한다.
	SurfaceTool UMETA(DisplayName = "Surface Tool"),

	// 지정 반경의 구 부피를 직접 더하거나 뺀다. SurfaceTool과 제거 느낌을 비교할 때 사용한다.
	SphereTool UMETA(DisplayName = "Sphere Tool")
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
	TObjectPtr<AVoxelWorld> TargetVoxelWorld = nullptr;

	// SnowVolumeSubsystem에는 같은 반경으로 팀별 density를 기록하고,
	// SnowSurfaceSubsystem에는 같은 반경으로 Voxel 표면 표현을 만든다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0"))
	float Amount = 1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	EDRSnowVoxelEditTool EditTool = EDRSnowVoxelEditTool::SurfaceTool;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	TObjectPtr<AVoxelWorld> TargetVoxelWorld = nullptr;

	// 흡수/제거가 영향을 주는 월드 반경이다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 100.f;

	// 현재는 Voxel 표면 제거 강도이며, 이후 SnowVolume density 감소량과 맞춰야 할 값이다.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow", meta = (ClampMin = "0.0"))
	float RequestedAmount = 1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	bool bInvertSurfaceStrength = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	EDRSnowVoxelEditTool EditTool = EDRSnowVoxelEditTool::SurfaceTool;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Snow")
	FDRSnowInteractionContext Context;
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
