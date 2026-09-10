#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameModeBase.h"
#include "Interfaces/IHttpRequest.h"
#include "RoomServiceTypes.h"
#include "RoomServiceGameModeBase.generated.h"

UENUM(BlueprintType)
enum class ERoomServiceState : uint8
{
	Waiting,
	Playing,
	Ending
};

// 프로젝트 GameMode는 이 클래스를 상속하고 상태만 보고하면 된다.
// -RoomId가 없는 기존 Listen/PIE/직접 접속 경로는 그대로 통과한다.
UCLASS()
class ROOMSERVICE_API ARoomServiceGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rooms")
	void SetRoomServiceState(ERoomServiceState NewState);
	UFUNCTION(BlueprintPure, Category = "Rooms")
	bool IsManagedRoom() const
	{
		return bManaged;
	}

	virtual void PreLoginAsync(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, const FOnPreLoginCompleteDelegate& OnComplete) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController,
		const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

protected:
	// 첫 Ready 보고 전에 Master의 방 구조체를 전달받는다.
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FRoomServiceInfo RoomDefinition;
	UFUNCTION(BlueprintNativeEvent, Category = "Rooms")
	void OnRoomDefinitionReady(const FRoomServiceInfo& Definition);
	virtual void OnRoomDefinitionReady_Implementation(const FRoomServiceInfo& Definition);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bManaged = false;
	bool bMasterAcknowledged = false;
	bool bDefinitionReceived = false;
	ERoomServiceState RoomState = ERoomServiceState::Waiting;
	FString RoomId;
	FString Secret;
	FString MasterUrl;
	int32 MaxRoomPlayers = 0;
	int32 GamePort = 0;
	float HeartbeatInterval = 2.f;
	float LeaseTimeout = 15.f;
	double LastMasterAck = 0;
	FTimerHandle HeartbeatTimer;
	FHttpRequestPtr ReportRequest;
	TMap<FString, double> PendingTokens;
	TMap<TWeakObjectPtr<AController>, FString> PlayerTokens;

	void Heartbeat();
	void PrunePlayers();
	FHttpRequestRef MakeRequest() const;
	TSharedPtr<class FJsonObject> ServerBody(const FString& Operation) const;
};
