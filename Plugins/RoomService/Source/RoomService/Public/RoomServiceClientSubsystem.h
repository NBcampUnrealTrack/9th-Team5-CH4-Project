#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "RoomServiceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RoomServiceClientSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRoomListReceived,
	const TArray<FRoomServiceInfo>&, Rooms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRoomConnectionReceived,
	const FRoomServiceConnection&, Connection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRoomRequestFailed, const FString&, Error);

// WBP는 이 API/구조체만 사용한다. 인증 credential의 획득은 로그인 시스템이 담당한다.
UCLASS()
class ROOMSERVICE_API URoomServiceClientSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Rooms")
	FRoomListReceived OnRoomListReceived;
	UPROPERTY(BlueprintAssignable, Category = "Rooms")
	FRoomConnectionReceived OnConnectionReceived;
	UPROPERTY(BlueprintAssignable, Category = "Rooms")
	FRoomRequestFailed OnRequestFailed;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	bool bRequestPending = false;
	// 맵 이동 후에도 초대 UI가 전체 방 코드를 표시할 수 있도록 보관한다.
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FString LastJoinedRoomId;

	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void SetCredential(const FString& InCredential);
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void RequestRoomList();
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void CreateRoom(const FRoomServiceInfo& Definition);
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void QuickMatch(const FString& MapId);
	// MapId는 지정 맵 필터 또는 빈 방이 없을 때 생성할 맵이다.
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void QuickMatchWithMode(ERoomQuickMatchMode Mode, const FString& MapId);
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void JoinRoom(const FString& RoomId);
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	bool ConnectToRoom(const FRoomServiceConnection& Connection);
	// 취소된 요청의 Master 예약은 TTL로 회수한다.
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void CancelRequest();

	virtual void Deinitialize() override;

private:
	FString Credential;
	FHttpRequestPtr ActiveRequest;
	void Send(const FString& Operation, TSharedPtr<class FJsonObject> Body);
};
