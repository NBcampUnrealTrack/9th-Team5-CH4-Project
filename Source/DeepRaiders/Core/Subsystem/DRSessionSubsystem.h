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

	/// 세션 생성 성공 여부
	FOnSessionComplete OnCreateSessionComplete;
	/// 세션 조인 성공 여부
	FOnSessionComplete OnJoinSessionComplete;

	/// 세션 생성
	/// @param NumPublicConnections 세션에 공개적으로 연결할 수 있는 클라이언트 수
	/// @param MatchType 매치 타입
	/// @param InLoadLevelName 로드할 레벨 이름 없을 경우 로드 하지 않음
	void CreateSession(const int32 NumPublicConnections, const FName MatchType,
	                   const FName InLoadLevelName = NAME_None);

	/// 랜 환경에서 세션 찾기 후 조인
	void FindAndJoinSession();

	/// IP 주소로 세션 조인
	/// @param IPAddress 도메인 또는 IP 주소
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

	/// 입력 받은 주소를 IP 주소로 변환
	/// @param IPAddress 도메인 또는 IP 주소
	/// @param OutFinalConnectURL 변환된 IP 주소
	///	@return 변환 성공 여부
	static bool TryConvertDomainToIP(const FString& IPAddress, FString& OutFinalConnectURL);
};
