#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "RoomServiceSettings.generated.h"

// Master와 클라이언트/게임 서버가 같은 설정 계약을 사용한다.
UCLASS(Config = RoomService, DefaultConfig, meta = (DisplayName = "Room Service"))
class ROOMSERVICE_API URoomServiceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Client")
	FString MasterUrl = TEXT("http://127.0.0.1:7000");
	UPROPERTY(Config, EditAnywhere, Category = "Master")
	int32 MasterPort = 7000;
	UPROPERTY(Config, EditAnywhere, Category = "Master")
	FString BindAddress = TEXT("127.0.0.1");
	UPROPERTY(Config, EditAnywhere, Category = "Master")
	FString AdvertisedHost = TEXT("127.0.0.1");
	UPROPERTY(Config, EditAnywhere, Category = "Master")
	FString ServerExecutable;
	UPROPERTY(Config, EditAnywhere, Category = "Master")
	int32 FirstGamePort = 7100;
	UPROPERTY(Config, EditAnywhere, Category = "Master")
	int32 LastGamePort = 7199;
	UPROPERTY(Config, EditAnywhere, Category = "Master")
	int32 MaxRooms = 8;
	UPROPERTY(Config, EditAnywhere, Category = "Master")
	int32 MaxPlayers = 8;
	// 클라이언트는 MapId만 보내고 실제 실행 경로는 서버의 허용 목록에서 찾는다.
	UPROPERTY(Config, EditAnywhere, Category = "Master")
	TMap<FString, FString> Maps;
	UPROPERTY(Config, EditAnywhere, Category = "Timeout")
	float StartupTimeout = 90.f;
	UPROPERTY(Config, EditAnywhere, Category = "Timeout")
	float HeartbeatInterval = 2.f;
	UPROPERTY(Config, EditAnywhere, Category = "Timeout")
	float HeartbeatTimeout = 15.f;
	UPROPERTY(Config, EditAnywhere, Category = "Timeout")
	float ReservationTimeout = 30.f;
	UPROPERTY(Config, EditAnywhere, Category = "Timeout")
	float EmptyRoomTimeout = 60.f;
	UPROPERTY(Config, EditAnywhere, Category = "Timeout")
	float EndingTimeout = 10.f;
	UPROPERTY(Config, EditAnywhere, Category = "Timeout")
	float RequestTimeout = 110.f;
	// 기본 백엔드는 로컬 개발용 익명 인증이다. 외부 인증은 이 클래스를 교체한다.
	UPROPERTY(Config, EditAnywhere, Category = "Backend")
	FSoftClassPath BackendClass;
};
