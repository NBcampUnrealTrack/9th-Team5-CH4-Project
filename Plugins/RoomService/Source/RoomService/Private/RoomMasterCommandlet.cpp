#include "RoomMasterCommandlet.h"

#include "RoomServiceBackend.h"
#include "RoomServiceProtocol.h"
#include "RoomServiceSettings.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "IPAddress.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogRoomMaster, Log, All);

namespace
{
	using namespace RoomServiceProtocol;

	FString GetServerExecutable(const URoomServiceSettings* Settings)
	{
		return Settings->bUseEditorServer
			? FString(FPlatformProcess::ExecutablePath()) : Settings->ServerExecutable;
	}

	struct FReply
	{
		FHttpResultCallback Callback;
		FString Identity;
		double Deadline = 0;
		bool bDone = false;

		void Send(const FJson& Body, int32 Code = 200)
		{
			if (bDone)
			{
				return;
			}
			bDone = true;
			auto Response = FHttpServerResponse::Create(Encode(Body), TEXT("application/json"));
			Response->Code = static_cast<EHttpServerResponseCodes>(Code);
			Callback(MoveTemp(Response));
		}

		void Fail(const TCHAR* Error, int32 Code = 409)
		{
			FJson Body = Object();
			Body->SetStringField(TEXT("error"), Error);
			Send(Body, Code);
		}
	};

	struct FTicket
	{
		FString Token;
		double Deadline = 0;
		bool bConsumed = false;
		bool bActive = false;
	};

	struct FRoom
	{
		FRoomServiceInfo Info;
		FProcHandle Process;
		FString Secret;
		int32 Port = 0;
		double Started = 0;
		double LastReport = 0;
		double EmptySince = 0;
		double EndingSince = 0;
		bool bPublishing = false;
		bool bRegistered = false;
		bool bStopping = false;
		bool bHadPlayers = false;
		TArray<TSharedPtr<FReply>> Waiting;
		TMap<FString, FTicket> Tickets;

		int32 Occupied() const
		{
			int32 Count = Info.CurrentPlayers + Waiting.Num();
			for (const auto& Pair : Tickets)
			{
				Count += Pair.Value.bActive ? 0 : 1;
			}
			return Count;
		}
	};

	class FMaster : public TSharedFromThis<FMaster>
	{
	public:
		explicit FMaster(URoomServiceBackend* InBackend)
			: Backend(InBackend), Settings(GetDefault<URoomServiceSettings>())
		{
		}

		bool Handle(const FHttpServerRequest& Request, const FHttpResultCallback& Complete)
		{
			check(IsInGameThread());
			auto Reply = MakeShared<FReply>();
			Reply->Callback = Complete;
			Reply->Deadline = FPlatformTime::Seconds() + Settings->RequestTimeout;
			if (Replies.Num() >= 256 || Request.Body.IsEmpty() || Request.Body.Num() > 16384)
			{
				Reply->Fail(TEXT("request_limit"), 400);
				return true;
			}
			Replies.Add(Reply);
			FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()),
				Request.Body.Num());
			FJson Body = Decode(FString(Text.Length(), Text.Get()));
			if (!Body)
			{
				Reply->Fail(TEXT("invalid_json"), 400);
				return true;
			}
			const FString Operation = String(Body, TEXT("op"));
			if (Operation == TEXT("report") || Operation == TEXT("admit")
				|| Operation == TEXT("describe"))
			{
				HandleServer(Body, Reply, Operation);
				return true;
			}
			const TWeakPtr<FMaster> Weak = AsShared();
			Backend->Authenticate(String(Body, TEXT("credential")),
				[Weak, Body, Reply, Operation](bool bAllowed, FString Identity)
				{
					check(IsInGameThread());
					if (const auto Self = Weak.Pin(); Self && !Reply->bDone)
					{
						if (!bAllowed || Identity.IsEmpty())
						{
							Reply->Fail(TEXT("unauthorized"), 401);
							return;
						}
						Reply->Identity = MoveTemp(Identity);
						Self->HandleClient(Body, Reply, Operation);
					}
				});
			return true;
		}

		void Tick()
		{
			const double Now = FPlatformTime::Seconds();
			TickWatchers(Now);
			for (const auto& Reply : Replies)
			{
				if (Now > Reply->Deadline)
				{
					Reply->Fail(TEXT("request_timeout"), 504);
				}
			}
			Replies.RemoveAll([](const auto& Reply)
			{
				return Reply->bDone;
			});
			TArray<FString> Remove;
			for (auto& Pair : Rooms)
			{
				FRoom& Room = *Pair.Value;
				Room.Waiting.RemoveAll([](const auto& Reply)
				{
					return Reply->bDone;
				});
				for (auto It = Room.Tickets.CreateIterator(); It; ++It)
				{
					if (!It.Value().bActive && Now > It.Value().Deadline)
					{
						It.RemoveCurrent();
					}
				}
				if (!FPlatformProcess::IsProcRunning(Room.Process))
				{
					Remove.Add(Pair.Key);
					continue;
				}
				if (Room.Occupied() > 0)
				{
					Room.EmptySince = Now;
				}
				const bool bStartupExpired = !Room.bRegistered
					&& Now - Room.Started > Settings->StartupTimeout;
				const bool bHeartbeatExpired = Room.bRegistered
					&& Now - Room.LastReport > Settings->HeartbeatTimeout;
				const bool bEmptyExpired = Now - Room.EmptySince > Settings->EmptyRoomTimeout;
				// 첫 입장 대기는 유지하되, 사용한 방은 마지막 인원과 예약이 사라지면 정리한다.
				const bool bRoomVacated = Room.bHadPlayers && Room.Occupied() == 0;
				const bool bEndingExpired = Room.EndingSince > 0
					&& Now - Room.EndingSince > Settings->EndingTimeout;
				if (!Room.bStopping && (bStartupExpired || bHeartbeatExpired
					|| (Room.bRegistered && (bEmptyExpired || bRoomVacated)) || bEndingExpired))
				{
					Stop(Room);
				}
			}
			for (const FString& Id : Remove)
			{
				FRoom& Room = *Rooms[Id];
				Stop(Room);
				FPlatformProcess::CloseProc(Room.Process);
				// 프로세스 종료를 확인한 후에만 포트를 풀로 반환한다.
				Ports.Remove(Room.Port);
				Rooms.Remove(Id);
			}
		}

		void Shutdown()
		{
			for (const auto& Reply : Replies)
			{
				Reply->Fail(TEXT("master_shutdown"), 503);
			}
			for (auto& Pair : Rooms)
			{
				Stop(*Pair.Value);
				FPlatformProcess::CloseProc(Pair.Value->Process);
			}
			Rooms.Empty();
		}

	private:
		friend class FRoomMasterReservationTest;
		TStrongObjectPtr<URoomServiceBackend> Backend;
		const URoomServiceSettings* Settings;
		TMap<FString, TSharedPtr<FRoom>> Rooms;
		TSet<int32> Ports;
		TArray<TSharedPtr<FReply>> Replies;
		struct FWatcher
		{
			TSharedPtr<FReply> Reply;
			FString Cursor;
			double Deadline;
		};
		TArray<FWatcher> Watchers;
		TMap<FString, TMap<FString, FString>> Snapshots;
		TArray<FString> SnapshotOrder;
		FString ListCursor;
		double NextWatchTick = 0;

		void TickWatchers(double Now)
		{
			if (Now < NextWatchTick)
			{
				return;
			}
			NextWatchTick = Now + 0.2;
			TMap<FString, FString> Current;
			for (const auto& Pair : Rooms)
			{
				const FRoom& Room = *Pair.Value;
				if (Room.bRegistered && !Room.bStopping && !Room.Info.bPrivate)
				{
					Current.Add(Pair.Key, Encode(ToJson(Room.Info)));
				}
			}
			const auto* Previous = Snapshots.Find(ListCursor);
			bool bChanged = !Previous || Previous->Num() != Current.Num();
			if (!bChanged)
			{
				for (const auto& Pair : Current)
				{
					const FString* Old = Previous->Find(Pair.Key);
					if (!Old || *Old != Pair.Value)
					{
						bChanged = true;
						break;
					}
				}
			}
			if (bChanged)
			{
				ListCursor = FGuid::NewGuid().ToString(EGuidFormats::Digits);
				Snapshots.Add(ListCursor, Current);
				SnapshotOrder.Add(ListCursor);
				if (SnapshotOrder.Num() > 64)
				{
					Snapshots.Remove(SnapshotOrder[0]);
					SnapshotOrder.RemoveAt(0);
				}
			}
			for (auto& Watcher : Watchers)
			{
				if (Watcher.Reply->bDone || (Watcher.Cursor == ListCursor && Now < Watcher.Deadline))
				{
					continue;
				}
				const auto* Baseline = Snapshots.Find(Watcher.Cursor);
				TArray<TSharedPtr<FJsonValue>> Changed;
				TArray<TSharedPtr<FJsonValue>> Removed;
				for (const auto& Pair : Current)
				{
					const FString* Old = Baseline ? Baseline->Find(Pair.Key) : nullptr;
					if (!Old || *Old != Pair.Value)
					{
						Changed.Add(MakeShared<FJsonValueObject>(Decode(Pair.Value)));
					}
				}
				if (Baseline)
				{
					for (const auto& Pair : *Baseline)
					{
						if (!Current.Contains(Pair.Key))
						{
							Removed.Add(MakeShared<FJsonValueString>(Pair.Key));
						}
					}
				}
				FJson Result = Object();
				Result->SetStringField(TEXT("cursor"), ListCursor);
				Result->SetBoolField(TEXT("reset"), !Baseline);
				Result->SetArrayField(TEXT("changed"), Changed);
				Result->SetArrayField(TEXT("removed"), Removed);
				Watcher.Reply->Send(Result);
			}
			Watchers.RemoveAll([](const FWatcher& Watcher) { return Watcher.Reply->bDone; });
		}

		void Stop(FRoom& Room)
		{
			if (Room.bStopping)
			{
				return;
			}
			Room.bStopping = true;
			Backend->RemoveRoom(Room.Info.RoomId);
			for (const auto& Reply : Room.Waiting)
			{
				Reply->Fail(TEXT("room_unavailable"), 503);
			}
			Room.Waiting.Empty();
			if (FPlatformProcess::IsProcRunning(Room.Process))
			{
				FPlatformProcess::TerminateProc(Room.Process, true);
			}
			UE_LOG(LogRoomMaster, Display, TEXT("Room stopped: %s"), *Room.Info.RoomId);
		}

		bool CanJoin(const FRoom& Room) const
		{
			return Room.bRegistered && !Room.bStopping && Room.Info.State == TEXT("Waiting")
				&& FPlatformTime::Seconds() - Room.LastReport <= Settings->HeartbeatTimeout
				&& Room.Occupied() < Room.Info.MaxPlayers;
		}

		void HandleClient(const FJson& Body, const TSharedPtr<FReply>& Reply,
			const FString& Operation)
		{
			if (Operation == TEXT("watch"))
			{
				if (Watchers.Num() >= 128)
				{
					Reply->Fail(TEXT("watch_limit"), 503);
					return;
				}
				// 인증 이후 Public 목록만 구독한다. 오래된 커서는 전체 스냅샷으로 복구한다.
				const double Deadline = FPlatformTime::Seconds() + 20.0;
				Reply->Deadline = Deadline + 5.0;
				Watchers.Add({Reply, String(Body, TEXT("cursor")), Deadline});
				return;
			}
			if (Operation == TEXT("list"))
			{
				const TWeakPtr<FMaster> Weak = AsShared();
				Backend->DiscoverRooms(Reply->Identity, [Weak, Reply](TArray<FRoomServiceInfo> Found)
				{
					check(IsInGameThread());
					if (const auto Self = Weak.Pin(); Self && !Reply->bDone)
					{
						TArray<TSharedPtr<FJsonValue>> Values;
						for (const auto& Candidate : Found)
						{
							const auto* Entry = Self->Rooms.Find(Candidate.RoomId);
							if (Entry && (*Entry)->bRegistered && !(*Entry)->bStopping
								&& !(*Entry)->Info.bPrivate)
							{
								Values.Add(MakeShared<FJsonValueObject>(ToJson((*Entry)->Info)));
							}
						}
						FJson Result = Object();
						Result->SetArrayField(TEXT("rooms"), Values);
						Reply->Send(Result);
					}
				});
				return;
			}
			if (Operation == TEXT("join"))
			{
				const auto* Room = Rooms.Find(String(Body, TEXT("roomId")));
				if (!Room || !CanJoin(**Room))
				{
					Reply->Fail(TEXT("room_not_joinable"));
					return;
				}
				Reserve(**Room, Reply);
				return;
			}
			if (Operation != TEXT("create") && Operation != TEXT("quick"))
			{
				Reply->Fail(TEXT("unknown_operation"), 400);
				return;
			}
			FRoomServiceInfo Info;
			Info.MapId = String(Body, TEXT("mapId"));
			if (Operation == TEXT("create")
				&& (!ReadRoom(Body, Info) || !Settings->Maps.Contains(Info.MapId)))
			{
				Reply->Fail(TEXT("invalid_room_definition"), 400);
				return;
			}
			if (Operation == TEXT("quick"))
			{
				const FString MatchMode = String(Body, TEXT("matchMode"));
				if (!MatchMode.IsEmpty() && MatchMode != TEXT("any") && MatchMode != TEXT("map"))
				{
					Reply->Fail(TEXT("invalid_match_mode"), 400);
					return;
				}
				const bool bAnyMap = MatchMode == TEXT("any");
				Info.bPrivate = false;
				for (auto& Pair : Rooms)
				{
					FRoom& Room = *Pair.Value;
					if (Room.Info.bPrivate || (!bAnyMap && Room.Info.MapId != Info.MapId)
						|| Room.bStopping)
					{
						continue;
					}
					if (CanJoin(Room))
					{
						Reserve(Room, Reply);
						return;
					}
				}
				// 빈 방이 없으면 클라이언트에서 생성 화면을 연다.
				Reply->Fail(TEXT("no_joinable_room"));
				return;
			}
			Create(MoveTemp(Info), Reply);
		}

		void Create(FRoomServiceInfo Info, const TSharedPtr<FReply>& Reply)
		{
			if (Rooms.Num() >= Settings->MaxRooms)
			{
				Reply->Fail(TEXT("max_rooms"));
				return;
			}
			int32 Port = 0;
			ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			for (int32 Candidate = Settings->FirstGamePort;
				Candidate <= Settings->LastGamePort; ++Candidate)
			{
				if (Ports.Contains(Candidate))
				{
					continue;
				}
				FSocket* Probe = Sockets->CreateSocket(NAME_DGram, TEXT("RoomPortProbe"), false);
				const auto Address = Sockets->CreateInternetAddr();
				Address->SetAnyAddress();
				Address->SetPort(Candidate);
				const bool bAvailable = Probe && Probe->Bind(*Address);
				if (Probe)
				{
					Sockets->DestroySocket(Probe);
				}
				if (bAvailable)
				{
					Port = Candidate;
					break;
				}
			}
			if (!Port)
			{
				Reply->Fail(TEXT("no_game_port"));
				return;
			}
			Ports.Add(Port);
			auto Room = MakeShared<FRoom>();
			Room->Info = MoveTemp(Info);
			Room->Info.RoomId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Room->Info.State = TEXT("Starting");
			Room->Info.CurrentPlayers = 0;
			Room->Info.MaxPlayers = Room->Info.MaxPlayers > 0
				? FMath::Clamp(Room->Info.MaxPlayers, 1, Settings->MaxPlayers) : Settings->MaxPlayers;
			Room->Port = Port;
			Room->Secret = NewSecret();
			Room->Started = Room->LastReport = Room->EmptySince = FPlatformTime::Seconds();
			Room->Waiting.Add(Reply);
			FString Args = FString::Printf(
				TEXT("%s -port=%d -RoomId=%s -RoomSecret=%s -RoomMaster=http://127.0.0.1:%d ")
				TEXT("-RoomMaxPlayers=%d -RoomHeartbeat=%f -RoomLease=%f -unattended -nullrhi"),
				*Settings->Maps[Room->Info.MapId], Port, *Room->Info.RoomId, *Room->Secret,
				Settings->MasterPort, Room->Info.MaxPlayers, Settings->HeartbeatInterval,
				Settings->HeartbeatTimeout);
			if (Settings->bUseEditorServer)
			{
				// 프로젝트 경로가 첫 인자이고 그 다음에 맵이 와야 한다.
				const FString Project = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
				Args = FString::Printf(TEXT("\"%s\" %s -server"), *Project, *Args);
			}
			const FString Executable = GetServerExecutable(Settings);
			Room->Process = FPlatformProcess::CreateProc(*Executable, *Args,
				false, true, true, nullptr, 0, *FPaths::GetPath(Executable), nullptr);
			if (!Room->Process.IsValid())
			{
				Ports.Remove(Port);
				Reply->Fail(TEXT("server_launch_failed"), 503);
				return;
			}
			Rooms.Add(Room->Info.RoomId, Room);
			UE_LOG(LogRoomMaster, Display, TEXT("Room starting: %s port=%d"),
				*Room->Info.RoomId, Port);
		}

		void Reserve(FRoom& Room, const TSharedPtr<FReply>& Reply)
		{
			// 발급 콜백보다 먼저 정원을 예약한다. 모든 변경은 Game Thread에서 직렬 처리한다.
			const FString Slot = NewSecret();
			FTicket& Ticket = Room.Tickets.Add(Slot);
			Ticket.Deadline = FPlatformTime::Seconds() + Settings->ReservationTimeout;
			const FString Id = Room.Info.RoomId;
			const FString Endpoint = FString::Printf(TEXT("%s:%d"),
				*Settings->AdvertisedHost, Room.Port);
			const TWeakPtr<FMaster> Weak = AsShared();
			Backend->IssueConnection(Room.Info, Reply->Identity, Endpoint,
				[Weak, Id, Slot, Reply](bool bSuccess, FRoomServiceConnection Connection)
				{
					check(IsInGameThread());
					const auto Self = Weak.Pin();
					const auto* Entry = Self ? Self->Rooms.Find(Id) : nullptr;
					if (!Entry)
					{
						Reply->Fail(TEXT("room_unavailable"), 503);
						return;
					}
					FRoom& Current = **Entry;
					FTicket* Reserved = Current.Tickets.Find(Slot);
					if (!bSuccess || !Reserved || Reply->bDone || Current.bStopping
						|| Current.Info.State != TEXT("Waiting")
						|| Connection.RoomId != Id || !IsIdentifier(Connection.Token)
						|| Connection.Endpoint.IsEmpty())
					{
						Current.Tickets.Remove(Slot);
						Reply->Fail(TEXT("connection_issue_failed"), 503);
						return;
					}
					Reserved->Token = Connection.Token;
					FJson Result = Object();
					Result->SetStringField(TEXT("roomId"), Id);
					Result->SetStringField(TEXT("endpoint"), Connection.Endpoint);
					Result->SetStringField(TEXT("token"), Connection.Token);
					Reply->Send(Result);
				});
		}

		void HandleServer(const FJson& Body, const TSharedPtr<FReply>& Reply,
			const FString& Operation)
		{
			const FString Id = String(Body, TEXT("roomId"));
			const auto* Entry = Rooms.Find(Id);
			if (!Entry || (*Entry)->bStopping || String(Body, TEXT("secret")) != (*Entry)->Secret)
			{
				Reply->Fail(TEXT("unknown_server"), 403);
				return;
			}
			FRoom& Room = **Entry;
			if (Operation == TEXT("describe"))
			{
				Reply->Send(ToJson(Room.Info));
				return;
			}
			if (Operation == TEXT("admit"))
			{
				Admit(Room, Body, Reply);
				return;
			}
			const FString State = String(Body, TEXT("state"));
			int32 Count = 0;
			int32 Port = 0;
			const TArray<TSharedPtr<FJsonValue>>* Active = nullptr;
			if (!Body->TryGetNumberField(TEXT("players"), Count) || Count < 0
				|| Count > Room.Info.MaxPlayers || !Body->TryGetNumberField(TEXT("port"), Port)
				|| Port != Room.Port || !Body->TryGetArrayField(TEXT("tokens"), Active)
				|| Active->Num() != Count || (State != TEXT("Waiting") && State != TEXT("Playing")
					&& State != TEXT("Ending")))
			{
				Reply->Fail(TEXT("invalid_report"), 400);
				return;
			}
			TSet<FString> ActiveTokens;
			for (const auto& Value : *Active)
			{
				FString Token;
				if (!Value->TryGetString(Token) || ActiveTokens.Contains(Token))
				{
					Reply->Fail(TEXT("invalid_active_tokens"), 400);
					return;
				}
				ActiveTokens.Add(Token);
			}
			for (auto It = Room.Tickets.CreateIterator(); It; ++It)
			{
				if (ActiveTokens.Contains(It.Value().Token))
				{
					It.Value().bActive = true;
				}
				else if (It.Value().bActive)
				{
					It.RemoveCurrent();
				}
			}
			Room.LastReport = FPlatformTime::Seconds();
			Room.Info.CurrentPlayers = Count;
			Room.bHadPlayers |= Count > 0;
			// Ending은 종단 상태다. 늦게 도착한 Waiting 보고가 방을 다시 열지 않는다.
			if (Room.Info.State != TEXT("Ending"))
			{
				Room.Info.State = State;
			}
			if (State == TEXT("Ending") && Room.EndingSince == 0)
			{
				Room.EndingSince = Room.LastReport;
			}
			Reply->Send(Object());
			if (!Room.bPublishing)
			{
				Publish(Room);
			}
		}

		void Publish(FRoom& Room)
		{
			Room.bPublishing = true;
			const FString Id = Room.Info.RoomId;
			const TWeakPtr<FMaster> Weak = AsShared();
			Backend->PublishRoom(Room.Info, [Weak, Id](bool bSuccess)
			{
				check(IsInGameThread());
				const auto Self = Weak.Pin();
				const auto* Entry = Self ? Self->Rooms.Find(Id) : nullptr;
				if (!Entry || (*Entry)->bStopping)
				{
					if (Self)
					{
						Self->Backend->RemoveRoom(Id);
					}
					return;
				}
				FRoom& Current = **Entry;
				Current.bPublishing = false;
				if (!bSuccess)
				{
					Self->Stop(Current);
					return;
				}
				Current.bRegistered = true;
				TArray<TSharedPtr<FReply>> Waiting = MoveTemp(Current.Waiting);
				Current.Waiting.Empty();
				for (const auto& Reply : Waiting)
				{
					if (!Reply->bDone && Self->CanJoin(Current))
					{
						Self->Reserve(Current, Reply);
					}
					else
					{
						Reply->Fail(TEXT("room_not_joinable"));
					}
				}
			});
		}

		void Admit(FRoom& Room, const FJson& Body, const TSharedPtr<FReply>& Reply)
		{
			const FString Token = String(Body, TEXT("token"));
			for (auto& Pair : Room.Tickets)
			{
				FTicket& Ticket = Pair.Value;
				if (Token.IsEmpty() || Ticket.Token != Token || Ticket.bConsumed || Ticket.bActive
					|| FPlatformTime::Seconds() > Ticket.Deadline)
				{
					continue;
				}
				if (!Room.bRegistered || Room.Info.State != TEXT("Waiting"))
				{
					break;
				}
				// 검증 중에도 토큰을 점유하여 동일 토큰의 동시 입장을 차단한다.
				Ticket.bConsumed = true;
				Ticket.Deadline = FPlatformTime::Seconds() + Settings->ReservationTimeout;
				const FString Id = Room.Info.RoomId;
				const FString Slot = Pair.Key;
				const TWeakPtr<FMaster> Weak = AsShared();
				Backend->ValidateConnection(Token, Ticket.Token, [Weak, Id, Slot, Reply](bool bValid)
				{
					check(IsInGameThread());
					const auto Self = Weak.Pin();
					const auto* Current = Self ? Self->Rooms.Find(Id) : nullptr;
					const FTicket* Reserved = Current ? (*Current)->Tickets.Find(Slot) : nullptr;
					if (bValid && Current && Reserved && !(*Current)->bStopping
						&& (*Current)->Info.State == TEXT("Waiting")
						&& FPlatformTime::Seconds() <= Reserved->Deadline)
					{
						Reply->Send(Object());
					}
					else
					{
						Reply->Fail(TEXT("invalid_token"), 403);
					}
				});
				return;
			}
			Reply->Fail(TEXT("invalid_or_expired_token"), 403);
		}
	};

#if WITH_DEV_AUTOMATION_TESTS
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoomMasterReservationTest,
		"RoomService.Master.ReservationAndReplay",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FRoomMasterReservationTest::RunTest(const FString& Parameters)
	{
		auto Master = MakeShared<FMaster>(NewObject<ULocalRoomServiceBackend>());
		auto Room = MakeShared<FRoom>();
		Room->Info.RoomId = TEXT("testroom");
		Room->Info.MapId = TEXT("testmap");
		Room->Info.Title = TEXT("Test");
		Room->Info.State = TEXT("Waiting");
		Room->Info.MaxPlayers = 2;
		Room->Secret = TEXT("testsecret");
		Room->Port = 7100;
		Room->bRegistered = true;
		Room->LastReport = FPlatformTime::Seconds();
		Master->Rooms.Add(Room->Info.RoomId, Room);
		auto MakeReply = [](int32& Status)
		{
			auto Reply = MakeShared<FReply>();
			Reply->Callback = [&Status](TUniquePtr<FHttpServerResponse>&& Response)
			{
				Status = static_cast<int32>(Response->Code);
			};
			return Reply;
		};
		FJson Join = Object();
		Join->SetStringField(TEXT("roomId"), Room->Info.RoomId);
		int32 Status = 0;
		Master->HandleClient(Join, MakeReply(Status), TEXT("join"));
		TestEqual(TEXT("First reservation succeeds"), Status, 200);
		Master->HandleClient(Join, MakeReply(Status), TEXT("join"));
		TestEqual(TEXT("Second reservation succeeds"), Status, 200);
		Master->HandleClient(Join, MakeReply(Status), TEXT("join"));
		TestEqual(TEXT("Reserved seats prevent overbooking"), Status, 409);
		FJson Admission = Object();
		const FString Token = Room->Tickets.CreateConstIterator().Value().Token;
		Admission->SetStringField(TEXT("token"), Token);
		Master->Admit(*Room, Admission, MakeReply(Status));
		TestEqual(TEXT("Issued token accepted once"), Status, 200);
		Master->Admit(*Room, Admission, MakeReply(Status));
		TestEqual(TEXT("Consumed token replay rejected"), Status, 403);
		TestEqual(TEXT("Admission retains reservation until player report"), Room->Occupied(), 2);
		FJson Report = Object();
		Report->SetStringField(TEXT("roomId"), Room->Info.RoomId);
		Report->SetStringField(TEXT("secret"), Room->Secret);
		Report->SetStringField(TEXT("state"), TEXT("Waiting"));
		Report->SetNumberField(TEXT("port"), Room->Port);
		Report->SetNumberField(TEXT("players"), 1);
		Report->SetArrayField(TEXT("tokens"), { MakeShared<FJsonValueString>(Token) });
		Master->HandleServer(Report, MakeReply(Status), TEXT("report"));
		TestEqual(TEXT("Actual player report succeeds"), Status, 200);
		TestEqual(TEXT("Reported player is not counted twice"), Room->Occupied(), 2);
		Report->SetNumberField(TEXT("players"), 0);
		Report->SetArrayField(TEXT("tokens"), {});
		Master->HandleServer(Report, MakeReply(Status), TEXT("report"));
		TestEqual(TEXT("Logout report releases the actual player's seat"), Room->Occupied(), 1);
		FTicket& Expired = Room->Tickets.CreateIterator().Value();
		Expired.Deadline = FPlatformTime::Seconds() - 1;
		Admission->SetStringField(TEXT("token"), Expired.Token);
		Master->Admit(*Room, Admission, MakeReply(Status));
		TestEqual(TEXT("Expired unused token is rejected"), Status, 403);
		Room->Info.State = TEXT("Playing");
		Room->Tickets.Empty();
		TestFalse(TEXT("Playing excludes joining despite free seats"), Master->CanJoin(*Room));
		Room->Info.State = TEXT("Waiting");
		Room->LastReport -= GetDefault<URoomServiceSettings>()->HeartbeatTimeout + 1.f;
		TestFalse(TEXT("Stale heartbeat excludes joining"), Master->CanJoin(*Room));

		// 실제 요청 처리 경로로 맵 무관/지정 맵 필터를 비교한다. 프로세스는 실행하지 않는다.
		TStrongObjectPtr<URoomServiceSettings> TestSettings(NewObject<URoomServiceSettings>());
		TestSettings->Maps.Add(TEXT("requested"), TEXT("/Game/Maps/Requested"));
		TestSettings->MaxRooms = 1;
		Master->Settings = TestSettings.Get();
		Room->Info.MapId = TEXT("different");
		Room->LastReport = FPlatformTime::Seconds();
		Room->Info.CurrentPlayers = 0;
		Room->Tickets.Empty();
		FRoomServiceInfo Definition;
		Definition.MapId = TEXT("requested");
		Definition.Title = TEXT("Quick match");
		FJson Quick = ToJson(Definition);
		Quick->SetStringField(TEXT("matchMode"), TEXT("any"));
		Master->HandleClient(Quick, MakeReply(Status), TEXT("quick"));
		TestEqual(TEXT("AnyMap joins an available room on another map"), Status, 200);
		Room->Tickets.Empty();
		Quick->SetStringField(TEXT("matchMode"), TEXT("map"));
		Master->HandleClient(Quick, MakeReply(Status), TEXT("quick"));
		TestEqual(TEXT("SelectedMap reports no joinable room on other maps"), Status, 409);
		Room->Info.bPrivate = true;
		Quick->SetStringField(TEXT("matchMode"), TEXT("any"));
		Master->HandleClient(Quick, MakeReply(Status), TEXT("quick"));
		TestEqual(TEXT("AnyMap never joins Private rooms"), Status, 409);
		Room->Info.bPrivate = false;
		Room->Info.State = TEXT("Playing");
		Master->HandleClient(Quick, MakeReply(Status), TEXT("quick"));
		TestEqual(TEXT("AnyMap never joins Playing rooms"), Status, 409);

		// 프로세스를 실행하지 않고 목록 변경/대기/삭제/커서 복구 응답을 검증한다.
		FJson Watch = Object();
		Master->HandleClient(Watch, MakeReply(Status), TEXT("watch"));
		double WatchTime = FPlatformTime::Seconds();
		Master->TickWatchers(WatchTime);
		TestEqual(TEXT("First subscription receives snapshot"), Status, 200);
		const FString InitialCursor = Master->ListCursor;
		Watch->SetStringField(TEXT("cursor"), InitialCursor);
		Status = 0;
		Master->HandleClient(Watch, MakeReply(Status), TEXT("watch"));
		Master->TickWatchers(WatchTime += 1.0);
		TestEqual(TEXT("Unchanged rooms keep request waiting"), Status, 0);
		Room->Info.CurrentPlayers = 1;
		Master->TickWatchers(WatchTime += 1.0);
		TestEqual(TEXT("Player update completes subscription"), Status, 200);
		TestTrue(TEXT("Player update advances cursor"), Master->ListCursor != InitialCursor);
		Watch->SetStringField(TEXT("cursor"), Master->ListCursor);
		Status = 0;
		Master->HandleClient(Watch, MakeReply(Status), TEXT("watch"));
		Room->bStopping = true;
		Master->TickWatchers(WatchTime += 1.0);
		TestEqual(TEXT("Room removal completes subscription"), Status, 200);
		TestEqual(TEXT("Stopping rooms leave public snapshot"),
			Master->Snapshots[Master->ListCursor].Num(), 0);
		Watch->SetStringField(TEXT("cursor"), TEXT("expired"));
		Status = 0;
		Master->HandleClient(Watch, MakeReply(Status), TEXT("watch"));
		Master->TickWatchers(WatchTime += 1.0);
		TestEqual(TEXT("Unknown cursor recovers with snapshot"), Status, 200);
		return true;
	}
#endif
}

URoomMasterCommandlet::URoomMasterCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = false;
	LogToConsole = true;
}

int32 URoomMasterCommandlet::Main(const FString& Params)
{
	const auto* Settings = GetDefault<URoomServiceSettings>();
	const FString Executable = GetServerExecutable(Settings);
	if (Settings->bUseEditorServer)
	{
		const FString Project = FPaths::GetProjectFilePath();
		if (!WITH_EDITOR || Project.IsEmpty() || !FPaths::FileExists(Project)
			|| Project.Contains(TEXT("\"")))
		{
			UE_LOG(LogRoomMaster, Error, TEXT("Editor server requires a valid .uproject."));
			return 1;
		}
	}
	if (Settings->MasterPort < 1 || Settings->MasterPort > 65535
		|| Settings->FirstGamePort < 1 || Settings->LastGamePort > 65535
		|| Settings->FirstGamePort > Settings->LastGamePort || Settings->MaxRooms < 1
		|| Settings->MaxRooms > Settings->LastGamePort - Settings->FirstGamePort + 1
		|| Settings->MaxPlayers < 1 || Settings->MaxPlayers > 128 || Settings->Maps.IsEmpty()
		|| Settings->HeartbeatInterval < 0.5f
		|| Settings->HeartbeatTimeout <= Settings->HeartbeatInterval * 3.f
		|| Settings->StartupTimeout <= Settings->HeartbeatTimeout
		|| Settings->RequestTimeout <= Settings->StartupTimeout + 5.f
		|| Settings->ReservationTimeout <= Settings->HeartbeatTimeout
		|| Settings->EmptyRoomTimeout <= Settings->ReservationTimeout || Settings->EndingTimeout < 1.f
		|| !FPaths::FileExists(Executable)
		|| FPaths::IsRelative(Executable)
		|| Executable.Contains(TEXT("\"")))
	{
		UE_LOG(LogRoomMaster, Error, TEXT("Invalid RoomService settings; see README configuration."));
		return 1;
	}
	for (const auto& Pair : Settings->Maps)
	{
		if (!IsIdentifier(Pair.Key) || !FPackageName::IsValidLongPackageName(Pair.Value))
		{
			UE_LOG(LogRoomMaster, Error, TEXT("Invalid map allowlist entry: %s"), *Pair.Key);
			return 1;
		}
	}
	for (const TCHAR Char : Settings->AdvertisedHost)
	{
		if (!FChar::IsAlnum(Char) && Char != TEXT('.') && Char != TEXT('-'))
		{
			UE_LOG(LogRoomMaster, Error, TEXT("AdvertisedHost must be an IPv4 address or hostname."));
			return 1;
		}
	}
	// 자체 서버는 loopback으로 보고한다. 외부 공개 시 0.0.0.0으로 수신한다.
	if (Settings->AdvertisedHost.IsEmpty() || (Settings->BindAddress != TEXT("127.0.0.1")
		&& Settings->BindAddress != TEXT("0.0.0.0")))
	{
		return 1;
	}
	UClass* BackendClass = ULocalRoomServiceBackend::StaticClass();
	if (!Settings->BackendClass.IsNull())
	{
		BackendClass = Settings->BackendClass.TryLoadClass<URoomServiceBackend>();
	}
	if (!BackendClass || BackendClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return 1;
	}
	auto Master = MakeShared<FMaster>(NewObject<URoomServiceBackend>(this, BackendClass));
	GConfig->SetString(TEXT("HTTPServer.Listeners"), TEXT("DefaultBindAddress"),
		*Settings->BindAddress, GEngineIni);
	auto& Http = FHttpServerModule::Get();
	// GetHttpRouter의 bind 실패 검사는 listener가 활성화되어 있어야 동작한다.
	Http.StartAllListeners();
	const auto Router = Http.GetHttpRouter(Settings->MasterPort, true);
	if (!Router)
	{
		UE_LOG(LogRoomMaster, Error, TEXT("Master port could not be bound."));
		Http.StopAllListeners();
		return 1;
	}
	// HTTPServer의 TDelegate 핸들러에 람다를 명시적으로 바인딩한다.
	const FHttpRequestHandler Handler = FHttpRequestHandler::CreateLambda(
		[Master](const FHttpServerRequest& Request, const FHttpResultCallback& Complete)
		{
			return Master->Handle(Request, Complete);
		});
	const auto Route = Router->BindRoute(FHttpPath(TEXT("/rooms")),
		EHttpServerRequestVerbs::VERB_POST, Handler);
	if (!Route)
	{
		Http.StopAllListeners();
		return 1;
	}
	UE_LOG(LogRoomMaster, Display, TEXT("Room Master listening on %s:%d"),
		*Settings->BindAddress, Settings->MasterPort);
	double Previous = FPlatformTime::Seconds();
	while (!IsEngineExitRequested())
	{
		const double Now = FPlatformTime::Seconds();
		FTSTicker::GetCoreTicker().Tick(static_cast<float>(Now - Previous));
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		Previous = Now;
		Master->Tick();
		FPlatformProcess::Sleep(0.01f);
	}
	Master->Shutdown();
	Router->UnbindRoute(Route);
	Http.StopAllListeners();
	return 0;
}
