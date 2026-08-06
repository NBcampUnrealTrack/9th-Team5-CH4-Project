#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "DRSessionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionComplete, bool, bWasSuccessful);

UCLASS()
class DEEPRAIDERS_API UDRSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UDRSessionSubsystem();

	FOnSessionComplete OnCreateSessionComplete;
	FOnSessionComplete OnJoinSessionComplete;

	void CreateSession(const int32 NumPublicConnections, const FName MatchType, const FName InLoadLevelName = NAME_None);
	void FindAndJoinSession();
	void JoinSession(const FString& IPAddress);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// ReSharper disable once CppBoundToDelegateMethodIsNotMarkedAsUFunction
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FName LoadLevelName;

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
};
