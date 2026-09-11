#pragma once

#include "CoreMinimal.h"
#include "RoomServiceTypes.h"
#include "UObject/Object.h"
#include "RoomServiceBackend.generated.h"

using FRoomAuthComplete = TFunction<void(bool, FString)>;
using FRoomPublishComplete = TFunction<void(bool)>;
using FRoomDiscoveryComplete = TFunction<void(TArray<FRoomServiceInfo>)>;
using FRoomConnectionComplete = TFunction<void(bool, FRoomServiceConnection)>;
using FRoomValidationComplete = TFunction<void(bool)>;

// 콜백은 Game Thread에서 정확히 한 번 호출한다. EOS 어댑터도 같은 계약을 지킨다.
// 프로세스/포트/정원 예약은 Master 소유이며 외부 디렉터리 결과를 신뢰하지 않는다.
UCLASS(Abstract)
class ROOMSERVICE_API URoomServiceBackend : public UObject
{
	GENERATED_BODY()

public:
	virtual void Authenticate(const FString& Credential, FRoomAuthComplete Complete);
	virtual void PublishRoom(const FRoomServiceInfo& Room, FRoomPublishComplete Complete);
	virtual void RemoveRoom(const FString& RoomId);
	virtual void DiscoverRooms(const FString& Identity, FRoomDiscoveryComplete Complete);
	virtual void IssueConnection(const FRoomServiceInfo& Room, const FString& Identity,
		const FString& Endpoint, FRoomConnectionComplete Complete);
	virtual void ValidateConnection(const FString& Presented, const FString& Issued,
		FRoomValidationComplete Complete);
};

// 자체 Master 기본 구현. 인증된 계정이 아닌 로컬 익명 사용자로 취급한다.
UCLASS()
class ROOMSERVICE_API ULocalRoomServiceBackend : public URoomServiceBackend
{
	GENERATED_BODY()

public:
	virtual void Authenticate(const FString& Credential, FRoomAuthComplete Complete) override;
	virtual void PublishRoom(const FRoomServiceInfo& Room, FRoomPublishComplete Complete) override;
	virtual void RemoveRoom(const FString& RoomId) override;
	virtual void DiscoverRooms(const FString& Identity, FRoomDiscoveryComplete Complete) override;
	virtual void IssueConnection(const FRoomServiceInfo& Room, const FString& Identity,
		const FString& Endpoint, FRoomConnectionComplete Complete) override;
	virtual void ValidateConnection(const FString& Presented, const FString& Issued,
		FRoomValidationComplete Complete) override;

private:
	TMap<FString, FRoomServiceInfo> Directory;
};
