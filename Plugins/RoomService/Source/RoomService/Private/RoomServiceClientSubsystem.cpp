#include "RoomServiceClientSubsystem.h"

#include "RoomServiceProtocol.h"
#include "RoomServiceSettings.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

using namespace RoomServiceProtocol;

void URoomServiceClientSubsystem::SetCredential(const FString& InCredential)
{
	Credential = InCredential;
}

void URoomServiceClientSubsystem::RequestRoomList()
{
	Send(TEXT("list"), Object());
}

void URoomServiceClientSubsystem::CreateRoom(const FRoomServiceInfo& Definition)
{
	Send(TEXT("create"), ToJson(Definition));
}

void URoomServiceClientSubsystem::QuickMatch(const FString& MapId)
{
	QuickMatchWithMode(ERoomQuickMatchMode::SelectedMap, MapId);
}

void URoomServiceClientSubsystem::QuickMatchWithMode(ERoomQuickMatchMode Mode, const FString& MapId)
{
	FRoomServiceInfo Definition;
	Definition.MapId = MapId;
	Definition.Title = MapId;
	FJson Body = ToJson(Definition);
	Body->SetStringField(TEXT("matchMode"), Mode == ERoomQuickMatchMode::AnyMap
		? TEXT("any") : TEXT("map"));
	Send(TEXT("quick"), Body);
}

void URoomServiceClientSubsystem::JoinRoom(const FString& RoomId)
{
	FJson Body = Object();
	Body->SetStringField(TEXT("roomId"), RoomId);
	Send(TEXT("join"), Body);
}

void URoomServiceClientSubsystem::Send(const FString& Operation, FJson Body)
{
	if (bRequestPending)
	{
		OnRequestFailed.Broadcast(TEXT("request_in_progress"));
		return;
	}
	const auto* Settings = GetDefault<URoomServiceSettings>();
	Body->SetStringField(TEXT("op"), Operation);
	Body->SetStringField(TEXT("credential"), Credential);
	ActiveRequest = FHttpModule::Get().CreateRequest();
	ActiveRequest->SetURL(Settings->MasterUrl / TEXT("rooms"));
	ActiveRequest->SetVerb(TEXT("POST"));
	ActiveRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	ActiveRequest->SetContentAsString(Encode(Body));
	ActiveRequest->SetTimeout(Settings->RequestTimeout + 5.f);
	ActiveRequest->OnProcessRequestComplete().BindWeakLambda(this,
		[this, Operation](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			if (Request != ActiveRequest)
			{
				return;
			}
			ActiveRequest.Reset();
			bRequestPending = false;
			FJson Json = Response ? Decode(Response->GetContentAsString()) : nullptr;
			if (!bSuccess || !Response || Response->GetResponseCode() != 200 || !Json)
			{
				FString Error = String(Json, TEXT("error"));
				OnRequestFailed.Broadcast(Error.IsEmpty() ? TEXT("master_request_failed") : Error);
				return;
			}
			if (Operation == TEXT("list"))
			{
				const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
				if (!Json->TryGetArrayField(TEXT("rooms"), Values))
				{
					OnRequestFailed.Broadcast(TEXT("invalid_room_list"));
					return;
				}
				TArray<FRoomServiceInfo> Rooms;
				for (const auto& Value : *Values)
				{
					const FJson* RoomJson = nullptr;
					FRoomServiceInfo Room;
					if (!Value->TryGetObject(RoomJson) || !ReadRoom(*RoomJson, Room))
					{
						OnRequestFailed.Broadcast(TEXT("invalid_room_list"));
						return;
					}
					Rooms.Add(MoveTemp(Room));
				}
				OnRoomListReceived.Broadcast(Rooms);
				return;
			}
			FRoomServiceConnection Connection;
			Connection.RoomId = String(Json, TEXT("roomId"));
			Connection.Endpoint = String(Json, TEXT("endpoint"));
			Connection.Token = String(Json, TEXT("token"));
			if (!IsIdentifier(Connection.RoomId) || !IsIdentifier(Connection.Token)
				|| Connection.Endpoint.IsEmpty())
			{
				OnRequestFailed.Broadcast(TEXT("invalid_connection"));
				return;
			}
			OnConnectionReceived.Broadcast(Connection);
		});
	bRequestPending = true;
	if (!ActiveRequest->ProcessRequest())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest.Reset();
		bRequestPending = false;
		OnRequestFailed.Broadcast(TEXT("request_start_failed"));
	}
}

bool URoomServiceClientSubsystem::ConnectToRoom(const FRoomServiceConnection& Connection)
{
	APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController();
	FString Host;
	FString PortText;
	if (!Controller || !IsIdentifier(Connection.Token) || !IsIdentifier(Connection.RoomId)
		|| !Connection.Endpoint.Split(TEXT(":"), &Host, &PortText) || Host.IsEmpty()
		|| !PortText.IsNumeric() || FCString::Atoi(*PortText) < 1
		|| FCString::Atoi(*PortText) > 65535)
	{
		return false;
	}
	for (const TCHAR Char : Host)
	{
		if (!FChar::IsAlnum(Char) && Char != TEXT('.') && Char != TEXT('-'))
		{
			return false;
		}
	}
	const FString Url = Connection.Endpoint + TEXT("?RoomToken=") + Connection.Token;
	LastJoinedRoomId = Connection.RoomId;
	Controller->ClientTravel(Url, TRAVEL_Absolute);
	return true;
}

void URoomServiceClientSubsystem::CancelRequest()
{
	if (ActiveRequest)
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
	bRequestPending = false;
}

void URoomServiceClientSubsystem::Deinitialize()
{
	StopWatchingRooms();
	CancelRequest();
	Credential.Empty();
	Super::Deinitialize();
}

void URoomServiceClientSubsystem::StartWatchingRooms()
{
	if (WatchTicker.IsValid())
	{
		return;
	}
	WatchCursor.Empty();
	NextWatchRequest = 0;
	WatchTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			if (!WatchRequest && FPlatformTime::Seconds() >= NextWatchRequest)
			{
				SendWatch();
			}
			return true;
		}), 0.2f);
}

void URoomServiceClientSubsystem::StopWatchingRooms()
{
	FTSTicker::GetCoreTicker().RemoveTicker(WatchTicker);
	WatchTicker.Reset();
	if (WatchRequest)
	{
		WatchRequest->OnProcessRequestComplete().Unbind();
		WatchRequest->CancelRequest();
		WatchRequest.Reset();
	}
	WatchCursor.Empty();
}

void URoomServiceClientSubsystem::SendWatch()
{
	FJson Body = Object();
	Body->SetStringField(TEXT("op"), TEXT("watch"));
	Body->SetStringField(TEXT("credential"), Credential);
	Body->SetStringField(TEXT("cursor"), WatchCursor);
	WatchRequest = FHttpModule::Get().CreateRequest();
	WatchRequest->SetURL(GetDefault<URoomServiceSettings>()->MasterUrl / TEXT("rooms"));
	WatchRequest->SetVerb(TEXT("POST"));
	WatchRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	WatchRequest->SetContentAsString(Encode(Body));
	WatchRequest->SetTimeout(30.f);
	// 생성/입장 요청과 별도 채널이며, 실패 시 재시도 전에 전체 동기화를 예약한다.
	WatchRequest->OnProcessRequestComplete().BindWeakLambda(this,
		[this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			if (Request != WatchRequest)
			{
				return;
			}
			WatchRequest.Reset();
			WatchCursor.Empty();
			NextWatchRequest = FPlatformTime::Seconds() + 3.0;
			FJson Json = Response ? Decode(Response->GetContentAsString()) : nullptr;
			if (!bSuccess || !Response || Response->GetResponseCode() != 200 || !Json)
			{
				return;
			}
			const TArray<TSharedPtr<FJsonValue>>* ChangedValues = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* RemovedValues = nullptr;
			bool bReset = false;
			const FString Cursor = String(Json, TEXT("cursor"));
			if (Cursor.IsEmpty() || !Json->TryGetBoolField(TEXT("reset"), bReset)
				|| !Json->TryGetArrayField(TEXT("changed"), ChangedValues)
				|| !Json->TryGetArrayField(TEXT("removed"), RemovedValues))
			{
				return;
			}
			TArray<FRoomServiceInfo> Changed;
			TArray<FString> Removed;
			for (const auto& Value : *ChangedValues)
			{
				const FJson* RoomJson = nullptr;
				FRoomServiceInfo Info;
				if (!Value->TryGetObject(RoomJson) || !ReadRoom(*RoomJson, Info))
				{
					return;
				}
				Changed.Add(MoveTemp(Info));
			}
			for (const auto& Value : *RemovedValues)
			{
				FString Id;
				if (!Value->TryGetString(Id))
				{
					return;
				}
				Removed.Add(Id);
			}
			WatchCursor = Cursor;
			NextWatchRequest = 0;
			OnRoomListDelta.Broadcast(Changed, Removed, bReset);
		});
	if (!WatchRequest->ProcessRequest())
	{
		WatchRequest->OnProcessRequestComplete().Unbind();
		WatchRequest.Reset();
		WatchCursor.Empty();
		NextWatchRequest = FPlatformTime::Seconds() + 3.0;
	}
}
