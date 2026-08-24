#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DRSessionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDRSessionOperationComplete,
	bool,
	bWasSuccessful);

UCLASS()
class DEEPRAIDERS_API UDRSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FDRSessionOperationComplete OnCreateSessionComplete;

	UPROPERTY(BlueprintAssignable)
	FDRSessionOperationComplete OnJoinSessionComplete;

	// Dedicated Server에서만 로컬/LAN 세션을 등록합니다.
	UFUNCTION(BlueprintCallable, Category = "Online|Sessions")
	void CreateServerSession();

	// IP:Port 또는 Domain:Port 주소로 서버에 직접 접속합니다.
	UFUNCTION(BlueprintCallable, Category = "Online|Sessions")
	void JoinServer(const FString& Address);

	// 서버에서 지정한 맵으로 이동합니다. 접속 중인 클라이언트는 서버를 따라 이동합니다.
	UFUNCTION(BlueprintCallable, Category = "Online|Sessions")
	bool ServerTravel(const FString& MapPath);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// ReSharper disable once CppBoundToDelegateMethodIsNotMarkedAsUFunction
	IOnlineSessionPtr SessionInterface;

	FDelegateHandle CreateSessionCompleteDelegateHandle;

	bool bDedicatedSessionCreationRequested = false; // 세션 생성 중복 방지

	void CreateSessionInternal(const UWorld* ServerWorld, int32 MaxPlayers, const FString& ServerName, const FString& MatchType);

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void ClearSessionDelegateHandles();

	bool RefreshOnlineSubsystem();

	static bool TryResolveConnectAddress(const FString& Address, FString& OutResolvedAddress);
};
