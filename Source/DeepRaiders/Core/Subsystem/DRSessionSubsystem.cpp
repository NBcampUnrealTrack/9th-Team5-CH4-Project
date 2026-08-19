#include "DRSessionSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDRSession, Log, All);

namespace DRSessionKeys
{
	// Steam 전환 시 DefaultEngine.ini의 DefaultPlatformService 도 변경해야함
	static const FName LocalSubsystemName(TEXT("NULL")); //로컬용
	static const FName ServerName(TEXT("ServerName"));
	static const FName MapName(TEXT("MapName"));
	static const FName MatchType(TEXT("MatchType"));
	static const FName GameVersion(TEXT("GameVersion"));

	static const FString DefaultServerName(TEXT("DeepRaiders Dedicated Server"));
	static const FString DefaultMatchType(TEXT("Default"));
	static constexpr int32 DefaultMaxPlayers = 8;

	static FString GetNetModeName(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("스탠드얼론");
		case NM_DedicatedServer:
			return TEXT("전용서버");
		case NM_ListenServer:
			return TEXT("리슨서버");
		case NM_Client:
			return TEXT("클라이언트");
		default:
			return TEXT("알수없음");
		}
	}
}

void UDRSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!RefreshOnlineSubsystem())
	{
		UE_LOG(LogDRSession, Warning, TEXT("[Session] OSS 초기화 실패 하위시스템=NULL 직접접속=가능"));
		return;
	}

	UE_LOG(LogDRSession, Log, TEXT("[Session] OSS 준비 완료 하위시스템=%s"), *DRSessionKeys::LocalSubsystemName.ToString());
}

void UDRSessionSubsystem::Deinitialize()
{
	ClearSessionDelegateHandles();

	SessionInterface.Reset();
	bDedicatedSessionCreationRequested = false;

	Super::Deinitialize();
}

void UDRSessionSubsystem::CreateServerSession()
{
	const UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		UE_LOG(LogDRSession, Error, TEXT("[Session] 세션 생성 실패: 월드가 유효하지 않음"));
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	if (World->GetNetMode() != NM_DedicatedServer)
	{
		UE_LOG(
			LogDRSession,
			Warning,
			TEXT("[Session] 세션 생성 생략: 전용 서버에서만 생성 가능 모드=%s"),
			*DRSessionKeys::GetNetModeName(World->GetNetMode()));
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	if (bDedicatedSessionCreationRequested)
	{
		UE_LOG(LogDRSession, Warning, TEXT("[Session] 세션 생성 생략: 이미 요청됨"));
		return;
	}

	bDedicatedSessionCreationRequested = true;

	UE_LOG(
		LogDRSession,
		Log,
		TEXT("[Session] 세션 생성 요청 서버명=\"%s\" 매치타입=\"%s\" 최대인원=%d"),
		*DRSessionKeys::DefaultServerName,
		*DRSessionKeys::DefaultMatchType,
		DRSessionKeys::DefaultMaxPlayers);

	CreateSessionInternal(
		World,
		DRSessionKeys::DefaultMaxPlayers,
		DRSessionKeys::DefaultServerName,
		DRSessionKeys::DefaultMatchType);
}

void UDRSessionSubsystem::CreateSessionInternal(const UWorld* ServerWorld, int32 MaxPlayers, const FString& ServerName,
                                                const FString& MatchType)
{
	if (!IsValid(ServerWorld))
	{
		UE_LOG(LogDRSession, Error, TEXT("[Session] 세션 생성 실패: 서버 월드가 유효하지 않음"));
		HandleCreateSessionComplete(NAME_GameSession, false);
		return;
	}

	if (!RefreshOnlineSubsystem())
	{
		UE_LOG(LogDRSession, Warning,
		       TEXT("[Session] 세션 생성 실패: OSS NULL 세션 인터페이스 없음 직접접속=가능"));
		HandleCreateSessionComplete(NAME_GameSession, false);
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		UE_LOG(LogDRSession, Log, TEXT("[Session] 세션 생성 생략: 기존 GameSession 재사용"));
		HandleCreateSessionComplete(NAME_GameSession, true);
		return;
	}

	FString CurrentMapName = ServerWorld->GetMapName();
	CurrentMapName.RemoveFromStart(ServerWorld->StreamingLevelsPrefix);

	// 서버 목록/추후 Steam 세션에 넘길 최소 메타데이터만 유지합니다.
	FOnlineSessionSettings Settings;
	Settings.bIsDedicated = true;
	Settings.bIsLANMatch = true;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bAllowInvites = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = false;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.bShouldAdvertise = true;
	Settings.bUsesPresence = false;
	Settings.bUseLobbiesIfAvailable = false;

	Settings.Set(DRSessionKeys::ServerName, ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(DRSessionKeys::MapName, CurrentMapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(DRSessionKeys::MatchType, MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(DRSessionKeys::GameVersion, FString(TEXT("1.0.0")),
	             EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	UE_LOG(
		LogDRSession,
		Log,
		TEXT("[Session] 세션 생성 시작 이름=%s 하위시스템=%s 맵=\"%s\" 최대인원=%d"),
		*FName(NAME_GameSession).ToString(),
		*DRSessionKeys::LocalSubsystemName.ToString(),
		*CurrentMapName,
		Settings.NumPublicConnections);

	CreateSessionCompleteDelegateHandle =
		SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateUObject(
				this,
				&UDRSessionSubsystem::HandleCreateSessionComplete));

	if (!SessionInterface->CreateSession(0, NAME_GameSession, Settings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
			CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();

		UE_LOG(LogDRSession, Error, TEXT("[Session] 세션 생성 호출 실패 이름=%s"), *FName(NAME_GameSession).ToString());
		HandleCreateSessionComplete(NAME_GameSession, false);
	}
}

void UDRSessionSubsystem::JoinServer(const FString& Address)
{
	const UWorld* World = GetWorld();

	if (IsValid(World) && World->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogDRSession, Warning, TEXT("[Session] 접속 생략: 전용 서버는 접속할 수 없음"));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	const FString TrimmedAddress =
		Address.TrimStartAndEnd();

	if (TrimmedAddress.IsEmpty())
	{
		UE_LOG(LogDRSession, Warning, TEXT("[Session] 접속 실패: 주소가 비어 있음"));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	if (!TrimmedAddress.Contains(TEXT(":")))
	{
		UE_LOG(
			LogDRSession,
			Warning,
			TEXT("[Session] 접속 주소에 포트 없음 주소=\"%s\" 힌트=\"IP:Port 형식 사용 예: 127.0.0.1:17777\""),
			*TrimmedAddress);
	}

	FString ResolvedAddress;
	if (!TryResolveConnectAddress(TrimmedAddress, ResolvedAddress))
	{
		UE_LOG(LogDRSession, Error, TEXT("[Session] 접속 실패: 주소 변환 실패 주소=\"%s\""),
		       *TrimmedAddress);
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;

	if (!IsValid(PlayerController))
	{
		UE_LOG(LogDRSession, Error, TEXT("[Session] 접속 실패: 플레이어 컨트롤러가 유효하지 않음 주소=\"%s\""),
		       *ResolvedAddress);
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	UE_LOG(LogDRSession, Log, TEXT("[Session] 서버 접속 이동 입력=\"%s\" 변환주소=\"%s\""), *TrimmedAddress,
	       *ResolvedAddress);
	OnJoinSessionComplete.Broadcast(true);

	PlayerController->ClientTravel(ResolvedAddress, TRAVEL_Absolute);
}

bool UDRSessionSubsystem::ServerTravel(const FString& MapPath)
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		UE_LOG(LogDRSession, Error, TEXT("[Session] 서버 이동 실패: 월드가 유효하지 않음"));
		return false;
	}

	if (World->GetNetMode() != NM_DedicatedServer)
	{
		UE_LOG(
			LogDRSession,
			Warning,
			TEXT("[Session] 서버 이동 생략: 전용 서버에서만 실행 가능 모드=%s"),
			*DRSessionKeys::GetNetModeName(World->GetNetMode()));
		return false;
	}

	FString TravelMapPath = MapPath.TrimStartAndEnd();
	if (TravelMapPath.IsEmpty())
	{
		UE_LOG(LogDRSession, Warning, TEXT("[Session] 서버 이동 실패: 맵 경로가 비어 있음"));
		return false;
	}

	if (TravelMapPath.Contains(TEXT("?listen")))
	{
		TravelMapPath = TravelMapPath.Replace(TEXT("?listen"), TEXT(""));
		UE_LOG(LogDRSession, Warning, TEXT("[Session] 서버 이동 경로에서 listen 옵션 제거 맵=\"%s\""), *TravelMapPath);
	}

	if (World->IsInSeamlessTravel())
	{
		UE_LOG(LogDRSession, Warning, TEXT("[Session] 서버 이동 생략: 이미 다른 맵으로 이동 중"));
		return false;
	}

	const bool bTravelStarted = World->ServerTravel(TravelMapPath, true);
	if (bTravelStarted)
	{
		UE_LOG(LogDRSession, Log, TEXT("[Session] 서버 이동 시작 맵=\"%s\""), *TravelMapPath);
	}
	else
	{
		UE_LOG(LogDRSession, Error, TEXT("[Session] 서버 이동 실패 맵=\"%s\""), *TravelMapPath);
	}

	return bTravelStarted;
}

void UDRSessionSubsystem::HandleCreateSessionComplete(
	FName SessionName,
	bool bWasSuccessful)
{
	if (SessionInterface.IsValid() && CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
			CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
	}

	if (bWasSuccessful)
	{
		UE_LOG(LogDRSession, Log, TEXT("[Session] 세션 생성 성공 이름=%s"), *SessionName.ToString());
	}
	else
	{
		UE_LOG(LogDRSession, Error, TEXT("[Session] 세션 생성 실패 이름=%s"), *SessionName.ToString());
		bDedicatedSessionCreationRequested = false;
	}

	OnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UDRSessionSubsystem::ClearSessionDelegateHandles()
{
	if (SessionInterface.IsValid() && CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
			CreateSessionCompleteDelegateHandle);
	}

	CreateSessionCompleteDelegateHandle.Reset();
}

bool UDRSessionSubsystem::RefreshOnlineSubsystem()
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld(), DRSessionKeys::LocalSubsystemName);

	if (OnlineSubsystem == nullptr)
	{
		SessionInterface.Reset();
		UE_LOG(LogDRSession, Warning, TEXT("[Session] OSS 조회 실패 하위시스템=NULL"));
		return false;
	}

	SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogDRSession, Warning, TEXT("[Session] OSS 세션 인터페이스 없음 하위시스템=%s"),
		       *OnlineSubsystem->GetSubsystemName().ToString());
		return false;
	}

	return true;
}

bool UDRSessionSubsystem::TryResolveConnectAddress(const FString& Address, FString& OutResolvedAddress)
{
	FString CleanAddress = Address.TrimStartAndEnd();
	if (CleanAddress.IsEmpty())
	{
		return false;
	}

	if (CleanAddress.Contains(TEXT("://")))
	{
		int32 ProtocolIndex = INDEX_NONE;
		CleanAddress.FindChar(TEXT(':'), ProtocolIndex);
		CleanAddress = CleanAddress.RightChop(ProtocolIndex + 3);
	}

	FString Host = CleanAddress;
	FString PortSuffix;
	int32 LastColonIndex = INDEX_NONE;
	if (CleanAddress.FindLastChar(TEXT(':'), LastColonIndex))
	{
		Host = CleanAddress.Left(LastColonIndex);
		PortSuffix = CleanAddress.RightChop(LastColonIndex);
	}

	if (Host.IsEmpty())
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr)
	{
		return false;
	}

	TSharedRef<FInternetAddr> ResolvedAddr = SocketSubsystem->CreateInternetAddr();

	bool bIsValidIp = false;
	ResolvedAddr->SetIp(*Host, bIsValidIp);

	if (!bIsValidIp)
	{
		const FAddressInfoResult AddressInfo = SocketSubsystem->GetAddressInfo(
			*Host, nullptr, EAddressInfoFlags::Default, NAME_None, SOCKTYPE_Datagram);

		if (AddressInfo.Results.IsEmpty())
		{
			return false;
		}

		ResolvedAddr = AddressInfo.Results[0].Address;
		bIsValidIp = true;
	}

	if (!bIsValidIp)
	{
		return false;
	}

	OutResolvedAddress = FString::Printf(TEXT("%s%s"), *ResolvedAddr->ToString(false), *PortSuffix);
	return true;
}
