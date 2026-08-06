// ReSharper disable CppMemberFunctionMayBeConst
#include "DRSessionSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "SocketSubsystem.h"
#include "DeepRaiders/DeepRaiders.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"

UDRSessionSubsystem::UDRSessionSubsystem()
{
}

void UDRSessionSubsystem::CreateSession(const int32 NumPublicConnections, const FName MatchType, const FName InLoadLevelName)
{
	if (!SessionInterface.IsValid())
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr) //기존 세션이 남아있다면
		SessionInterface->DestroySession(NAME_GameSession);

	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsLANMatch = true;
	SessionSettings.NumPublicConnections = NumPublicConnections; //최대 입장 인원 수
	SessionSettings.bAllowJoinInProgress = true; //게임중 난입 허용
	SessionSettings.bAllowJoinViaPresence = true;
	SessionSettings.bShouldAdvertise = true; //서버 목록 노출 여부

	SessionSettings.bUsesPresence = true;
	SessionSettings.bUseLobbiesIfAvailable = true;

	SessionSettings.Set(FName("MatchType"), MatchType.ToString(),
	                    EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	LoadLevelName = InLoadLevelName;

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionSettings))
		OnCreateSessionComplete.Broadcast(false);
}

void UDRSessionSubsystem::FindAndJoinSession()
{
	if (!SessionInterface.IsValid()) return;

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->bIsLanQuery = true; //lan 매치 검색
	SessionSearch->MaxSearchResults = 20;

	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef()))
	{
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UDRSessionSubsystem::JoinSession(const FString& IPAddress)
{
	FString CleanedIP = IPAddress.TrimStartAndEnd();
	if (CleanedIP.IsEmpty()) return;

	if (CleanedIP.Contains(TEXT("://")))
	{
		int32 ProtocolIndex = CleanedIP.Find(TEXT("://"));
		CleanedIP = CleanedIP.RightChop(ProtocolIndex + 3);
	}

	FString HostDomain = CleanedIP;
	FString PortSuffix = TEXT(":7777"); // 기본 포트 지정
	int32 LastColonIndex;

	if (CleanedIP.FindLastChar(':', LastColonIndex))
	{
		HostDomain = CleanedIP.Left(LastColonIndex);
		PortSuffix = CleanedIP.RightChop(LastColonIndex);
	}

	if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		TSharedRef<FInternetAddr> ResolvedAddr = SocketSubsystem->CreateInternetAddr();
		bool bIsValidIP = false;

		ResolvedAddr->SetIp(*HostDomain, bIsValidIP);

		if (!bIsValidIP)
		{
			const TCHAR* HostNamePtr = HostDomain.GetCharArray().GetData();
			const FAddressInfoResult AddressInfo = SocketSubsystem->GetAddressInfo(
				HostNamePtr, nullptr, EAddressInfoFlags::Default, NAME_None, SOCKTYPE_Unknown
			);

			if (AddressInfo.Results.Num() > 0)
			{
				ResolvedAddr = AddressInfo.Results[0].Address;
				bIsValidIP = true;
			}
		}

		if (bIsValidIP)
		{
			HostDomain = ResolvedAddr->ToString(false);
		}
		else
		{
			DR_PRINT_ERROR(TEXT("[DNS 실패] 주소를 해석할 수 없습니다: %s"), *HostDomain);
			OnJoinSessionComplete.Broadcast(false);
			return;
		}
	}

	const FString FinalConnectURL = FString::Printf(TEXT("%s%s"), *HostDomain, *PortSuffix);

	DR_PRINT_LOG(TEXT("[접속 시도 URL] 최종 목적지: %s"), *FinalConnectURL);

	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		PlayerController->ClientTravel(FinalConnectURL, TRAVEL_Absolute);
	}
}

void UDRSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld()))
	{
		SessionInterface = Subsystem->GetSessionInterface();
		if (!SessionInterface.IsValid())
			return;

		SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(
			this, &UDRSessionSubsystem::HandleCreateSessionComplete);
		SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(
			this, &UDRSessionSubsystem::HandleFindSessionsComplete);
		SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(
			this, &UDRSessionSubsystem::HandleJoinSessionComplete);
	}
}

void UDRSessionSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDRSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	OnCreateSessionComplete.Broadcast(bWasSuccessful);

	UWorld* World = GetWorld();
	if (!World)
		return;

	if (bWasSuccessful)
	{
		if (!LoadLevelName.IsNone())
		{
			UGameplayStatics::OpenLevel(World, LoadLevelName, true, "listen");
		}
		else
		{
			FString CurrentMapName = World->GetMapName();
			CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix); //접두사 제거
			World->ServerTravel(FString::Printf(TEXT("%s?listen"), *CurrentMapName));
		}
	}

	LoadLevelName = NAME_None;
}

void UDRSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful || !SessionSearch.IsValid() || SessionSearch->SearchResults.Num() <= 0)
	{
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	FOnlineSessionSearchResult BestResult = SessionSearch->SearchResults[0]; // 상태 좋은 순으로 정렬되는듯

	BestResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, BestResult))
		OnJoinSessionComplete.Broadcast(false);
}

void UDRSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const bool bSuccess = Result == EOnJoinSessionCompleteResult::Success;
	OnJoinSessionComplete.Broadcast(bSuccess);

	if (bSuccess)
	{
		FString ConnectInfo;
		if (SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectInfo))
		{
			if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
				PlayerController->ClientTravel(ConnectInfo, TRAVEL_Absolute);
		}
	}
}
