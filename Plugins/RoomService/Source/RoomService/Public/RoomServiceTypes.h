#pragma once

#include "CoreMinimal.h"
#include "RoomServiceTypes.generated.h"

UENUM(BlueprintType)
enum class ERoomQuickMatchMode : uint8
{
	AnyMap UMETA(DisplayName = "Any available room"),
	SelectedMap UMETA(DisplayName = "Selected map")
};

USTRUCT(BlueprintType)
struct ROOMSERVICE_API FRoomServiceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FString RoomId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rooms")
	FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rooms")
	FString MapId;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FString State;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	int32 CurrentPlayers = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rooms")
	int32 MaxPlayers = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rooms")
	bool bPrivate = false;
	// 프로젝트별 아이콘 키/모드 등은 이 필드로 확장한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rooms")
	TMap<FString, FString> Attributes;
};

USTRUCT(BlueprintType)
struct ROOMSERVICE_API FRoomServiceConnection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FString RoomId;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FString Endpoint;
	// 일회용 토큰은 로그나 UI에 표시하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FString Token;
};
