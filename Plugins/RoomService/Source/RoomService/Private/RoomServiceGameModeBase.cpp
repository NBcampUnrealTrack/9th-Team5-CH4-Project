#include "RoomServiceGameModeBase.h"

#include "RoomServiceProtocol.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

using namespace RoomServiceProtocol;

void ARoomServiceGameModeBase::OnRoomDefinitionReady_Implementation(
	const FRoomServiceInfo& Definition)
{
}

void ARoomServiceGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	bManaged = IsRunningDedicatedServer()
		&& FParse::Value(FCommandLine::Get(), TEXT("RoomId="), RoomId);
	if (!bManaged)
	{
		return;
	}
	FParse::Value(FCommandLine::Get(), TEXT("RoomSecret="), Secret);
	FParse::Value(FCommandLine::Get(), TEXT("RoomMaster="), MasterUrl);
	FParse::Value(FCommandLine::Get(), TEXT("RoomMaxPlayers="), MaxRoomPlayers);
	FParse::Value(FCommandLine::Get(), TEXT("port="), GamePort);
	FParse::Value(FCommandLine::Get(), TEXT("RoomHeartbeat="), HeartbeatInterval);
	FParse::Value(FCommandLine::Get(), TEXT("RoomLease="), LeaseTimeout);
	if (!IsIdentifier(RoomId) || !IsIdentifier(Secret) || MasterUrl.IsEmpty()
		|| MaxRoomPlayers < 1 || GamePort < 1 || HeartbeatInterval < 0.5f
		|| LeaseTimeout <= HeartbeatInterval * 3.f)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (GameSession)
	{
		GameSession->MaxPlayers = MaxRoomPlayers;
	}
	LastMasterAck = FPlatformTime::Seconds();
	GetWorldTimerManager().SetTimer(HeartbeatTimer, this,
		&ARoomServiceGameModeBase::Heartbeat, HeartbeatInterval, true, 0.1f);
}

FJson ARoomServiceGameModeBase::ServerBody(const FString& Operation) const
{
	FJson Body = Object();
	Body->SetStringField(TEXT("op"), Operation);
	Body->SetStringField(TEXT("roomId"), RoomId);
	Body->SetStringField(TEXT("secret"), Secret);
	return Body;
}

FHttpRequestRef ARoomServiceGameModeBase::MakeRequest() const
{
	auto Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(MasterUrl / TEXT("rooms"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetTimeout(FMath::Min(5.f, LeaseTimeout / 3.f));
	return Request;
}

void ARoomServiceGameModeBase::PrunePlayers()
{
	for (auto It = PendingTokens.CreateIterator(); It; ++It)
	{
		if (FPlatformTime::Seconds() > It.Value())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = PlayerTokens.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ARoomServiceGameModeBase::Heartbeat()
{
	if (!bManaged)
	{
		return;
	}
	// Master가 죽거나 재시작되면 고아 프로세스가 포트를 계속 점유하지 않는다.
	if (FPlatformTime::Seconds() - LastMasterAck > LeaseTimeout)
	{
		bMasterAcknowledged = false;
		FPlatformMisc::RequestExit(false);
		return;
	}
	UNetDriver* Driver = GetWorld()->GetNetDriver();
	if (ReportRequest || !Driver || !Driver->IsNetResourceValid()
		|| !Driver->GetLocalAddr() || Driver->GetLocalAddr()->GetPort() != GamePort)
	{
		return;
	}
	PrunePlayers();
	const bool bDescribe = !bDefinitionReceived;
	FJson Body = ServerBody(bDescribe ? TEXT("describe") : TEXT("report"));
	const TCHAR* State = RoomState == ERoomServiceState::Waiting ? TEXT("Waiting")
		: RoomState == ERoomServiceState::Playing ? TEXT("Playing") : TEXT("Ending");
	Body->SetStringField(TEXT("state"), State);
	Body->SetNumberField(TEXT("port"), GamePort);
	Body->SetNumberField(TEXT("players"), PlayerTokens.Num());
	RoomDefinition.State = State;
	RoomDefinition.CurrentPlayers = PlayerTokens.Num();
	TArray<TSharedPtr<FJsonValue>> Tokens;
	for (const auto& Pair : PlayerTokens)
	{
		Tokens.Add(MakeShared<FJsonValueString>(Pair.Value));
	}
	Body->SetArrayField(TEXT("tokens"), Tokens);
	ReportRequest = MakeRequest();
	ReportRequest->SetContentAsString(Encode(Body));
	ReportRequest->OnProcessRequestComplete().BindWeakLambda(this,
		[this, bDescribe](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			ReportRequest.Reset();
			if (bSuccess && Response && Response->GetResponseCode() == 200)
			{
				if (bDescribe)
				{
					FRoomServiceInfo Definition;
					if (!ReadRoom(Decode(Response->GetContentAsString()), Definition)
						|| Definition.RoomId != RoomId || Definition.MaxPlayers != MaxRoomPlayers)
					{
						FPlatformMisc::RequestExit(false);
						return;
					}
					RoomDefinition = MoveTemp(Definition);
					bDefinitionReceived = true;
				}
				LastMasterAck = FPlatformTime::Seconds();
				bMasterAcknowledged = true;
				if (bDescribe)
				{
					OnRoomDefinitionReady(RoomDefinition);
					Heartbeat();
				}
			}
			else if (Response && Response->GetResponseCode() == 403)
			{
				bMasterAcknowledged = false;
				FPlatformMisc::RequestExit(false);
			}
		});
	if (!ReportRequest->ProcessRequest())
	{
		ReportRequest->OnProcessRequestComplete().Unbind();
		ReportRequest.Reset();
	}
}

void ARoomServiceGameModeBase::SetRoomServiceState(ERoomServiceState NewState)
{
	if (RoomState != ERoomServiceState::Ending)
	{
		RoomState = NewState;
	}
	Heartbeat();
}

void ARoomServiceGameModeBase::PreLoginAsync(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, const FOnPreLoginCompleteDelegate& OnComplete)
{
	if (!bManaged)
	{
		Super::PreLoginAsync(Options, Address, UniqueId, OnComplete);
		return;
	}
	FString Error;
	PreLogin(Options, Address, UniqueId, Error);
	PrunePlayers();
	const FString Token = UGameplayStatics::ParseOption(Options, TEXT("RoomToken"));
	if (!Error.IsEmpty() || RoomState != ERoomServiceState::Waiting
		|| !IsIdentifier(Token) || PendingTokens.Contains(Token)
		|| PlayerTokens.FindKey(Token) || PlayerTokens.Num() + PendingTokens.Num() >= MaxRoomPlayers)
	{
		OnComplete.ExecuteIfBound(Error.IsEmpty() ? TEXT("room_not_joinable") : Error);
		return;
	}
	// HTTP 검증 중인 접속도 로컬 정원에 포함한다.
	PendingTokens.Add(Token, FPlatformTime::Seconds() + LeaseTimeout);
	FJson Body = ServerBody(TEXT("admit"));
	Body->SetStringField(TEXT("token"), Token);
	auto Request = MakeRequest();
	Request->SetContentAsString(Encode(Body));
	const TWeakObjectPtr<ARoomServiceGameModeBase> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, Token, OnComplete](FHttpRequestPtr HttpRequest,
			FHttpResponsePtr Response, bool bSuccess)
		{
			auto* Self = WeakThis.Get();
			if (!Self)
			{
				OnComplete.ExecuteIfBound(TEXT("room_closed"));
				return;
			}
			const double* Deadline = Self->PendingTokens.Find(Token);
			if (!bSuccess || !Response || Response->GetResponseCode() != 200 || !Deadline
				|| FPlatformTime::Seconds() > *Deadline
				|| Self->RoomState != ERoomServiceState::Waiting)
			{
				Self->PendingTokens.Remove(Token);
				OnComplete.ExecuteIfBound(TEXT("room_admission_failed"));
				return;
			}
			Self->bMasterAcknowledged = true;
			Self->LastMasterAck = FPlatformTime::Seconds();
			OnComplete.ExecuteIfBound(FString());
		});
	if (!Request->ProcessRequest())
	{
		Request->OnProcessRequestComplete().Unbind();
		PendingTokens.Remove(Token);
		OnComplete.ExecuteIfBound(TEXT("master_unreachable"));
	}
}

FString ARoomServiceGameModeBase::InitNewPlayer(APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	if (!bManaged)
	{
		return Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	}
	const FString Token = UGameplayStatics::ParseOption(Options, TEXT("RoomToken"));
	const double* Deadline = PendingTokens.Find(Token);
	if (!Deadline || FPlatformTime::Seconds() > *Deadline || !bMasterAcknowledged
		|| RoomState != ERoomServiceState::Waiting || PlayerTokens.Num() >= MaxRoomPlayers)
	{
		PendingTokens.Remove(Token);
		return TEXT("room_admission_expired");
	}
	PendingTokens.Remove(Token);
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (Error.IsEmpty())
	{
		PlayerTokens.Add(NewPlayerController, Token);
	}
	return Error;
}

void ARoomServiceGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	Heartbeat();
}

void ARoomServiceGameModeBase::Logout(AController* Exiting)
{
	PlayerTokens.Remove(Exiting);
	Super::Logout(Exiting);
	Heartbeat();
}

void ARoomServiceGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetRoomServiceState(ERoomServiceState::Ending);
	GetWorldTimerManager().ClearTimer(HeartbeatTimer);
	if (ReportRequest)
	{
		ReportRequest->OnProcessRequestComplete().Unbind();
		ReportRequest->CancelRequest();
		ReportRequest.Reset();
	}
	bMasterAcknowledged = false;
	Super::EndPlay(EndPlayReason);
}
