#include "DRRoomServiceWidget.h"

#include "DRCreateRoomWidget.h"
#include "DeepRaiders/UI/Title/DRTitleJoinWidget.h"
#include "DRRoomEntryWidget.h"
#include "DeepRaiders/UI/Title/DRTitleMapDefinition.h"
#include "Components/EditableTextBox.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "RoomServiceClientSubsystem.h"
#include "RoomServiceProtocol.h"
#include "RoomServiceSettings.h"

void UDRRoomServiceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RoomSubsystem = GetGameInstance()->GetSubsystem<URoomServiceClientSubsystem>();
	if (RoomSubsystem)
	{
		RoomSubsystem->OnRoomListReceived.AddUniqueDynamic(this, &ThisClass::HandleRoomListReceived);
		RoomSubsystem->OnConnectionReceived.AddUniqueDynamic(this, &ThisClass::HandleConnectionReceived);
		RoomSubsystem->OnRequestFailed.AddUniqueDynamic(this, &ThisClass::HandleRequestFailed);
	}
	if (GEngine && !NetworkFailureHandle.IsValid())
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
			this, &ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
			this, &ThisClass::HandleTravelFailure);
	}
	if (PrivateJoinPanel)
	{
		PrivateJoinPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	bPrivatePanelOpen = false;
	RefreshEnabledState();
}

void UDRRoomServiceWidget::Open(UUserWidget* InReturnWidget, UWidget* InTitleContent,
	UDRCreateRoomWidget* InCreatePanel, UDRTitleJoinWidget* InListenJoinPanel)
{
	// 자체 Visibility만으로는 부모에 가려진 상태를 판단할 수 없어 항상 표시를 적용한다.
	UE_LOG(LogTemp, Log, TEXT("[RoomUI] Open %s. TitleContent=%s"),
		*GetName(), *GetNameSafe(InTitleContent));
	ReturnWidget = InReturnWidget;
	TitleContent = InTitleContent;
	CreatePanel = InCreatePanel;
	ListenJoinPanel = InListenJoinPanel;
	if (TitleContent.IsValid())
	{
		TitleContent->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::Visible);
	FocusRoomScreen();
	RefreshRooms();
}

void UDRRoomServiceWidget::FocusRoomScreen()
{
	SetIsFocusable(true);
	if (APlayerController* Controller = GetOwningPlayer())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(Mode);
		Controller->SetShowMouseCursor(true);
	}
}

bool UDRRoomServiceWidget::BeginRequest(const FText& Message)
{
	if (bBusy || bConnecting)
	{
		return false;
	}
	if (!RoomSubsystem || RoomSubsystem->bRequestPending)
	{
		SetStatus(NSLOCTEXT("Rooms", "BusyElsewhere", "다른 요청이 진행 중이거나 서비스가 없습니다."));
		return false;
	}
	// 요청 실패가 동기적으로 돌아올 수 있으므로 호출 전에 소유권/로딩을 설정한다.
	bOwnsRequest = true;
	bBusy = true;
	SetStatus(Message);
	RefreshEnabledState();
	return true;
}

void UDRRoomServiceWidget::SetStatus(const FText& Message)
{
	StatusMessage = Message;
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
	OnStatusChanged(Message);
	if (CreatePanel)
	{
		CreatePanel->SetFeedback(Message);
	}
}

void UDRRoomServiceWidget::RefreshEnabledState()
{
	const bool bActionsEnabled = !bBusy && !bCreatePanelOpen && !bPrivatePanelOpen;
	RoomListView->SetIsEnabled(bActionsEnabled);
	QuickMatch->SetIsEnabled(bActionsEnabled);
	CreateRoom->SetIsEnabled(!bConnecting && !bCreatePanelOpen && !bPrivatePanelOpen);
	PrivateJoin->SetIsEnabled(!bConnecting && !bCreatePanelOpen && ListenJoinPanel);
	// 접속 명령을 보낸 뒤에는 HTTP 취소만으로 이동을 취소할 수 없다.
	Exit->SetIsEnabled(!bConnecting && !bCreatePanelOpen && !bPrivatePanelOpen);
	if (RoomCodeInput)
	{
		RoomCodeInput->SetIsEnabled(!bBusy);
	}
	if (CreatePanel)
	{
		CreatePanel->SetSubmitting(bBusy);
	}
}

void UDRRoomServiceWidget::RefreshRooms()
{
	if (!bCreatePanelOpen && !bPrivatePanelOpen
		&& BeginRequest(NSLOCTEXT("Rooms", "Fetching", "방 목록을 불러오고 있습니다…")))
	{
		RoomSubsystem->RequestRoomList();
	}
}

void UDRRoomServiceWidget::HandleQuickMatchClicked()
{
	if (!GetDefault<URoomServiceSettings>()->Maps.Contains(QuickMatchMapId))
	{
		SetStatus(NSLOCTEXT("Rooms", "QuickMapMissing", "퀵매치 맵 설정을 확인해 주세요."));
		return;
	}
	if (!bCreatePanelOpen && !bPrivatePanelOpen
		&& BeginRequest(NSLOCTEXT("Rooms", "Matching", "참가할 방을 찾고 있습니다…")))
	{
		RoomSubsystem->QuickMatchWithMode(QuickMatchMode, QuickMatchMapId);
	}
}

void UDRRoomServiceWidget::HandleCreateRoomClicked()
{
	if (bConnecting || bCreatePanelOpen || bPrivatePanelOpen)
	{
		return;
	}
	if (CreatePanel)
	{
		// Master 응답을 기다리는 중에도 Private 리슨 방을 만들 수 있다.
		CancelOwnedRequest();
		bCreatePanelOpen = true;
		CreatePanel->Open(this);
		SetStatus(FText::GetEmpty());
		RefreshEnabledState();
	}
}

void UDRRoomServiceWidget::SubmitCreateRoom(const FRoomServiceInfo& Definition)
{
	if (!bCreatePanelOpen)
	{
		return;
	}
	if (Definition.Title.IsEmpty() || Definition.Title.Len() > 80)
	{
		SetStatus(NSLOCTEXT("Rooms", "BadTitle", "방 제목을 1~80자로 입력해 주세요."));
		return;
	}
	if (!GetDefault<URoomServiceSettings>()->Maps.Contains(Definition.MapId))
	{
		SetStatus(NSLOCTEXT("Rooms", "BadMap", "서버 설정에 등록된 맵을 선택해 주세요."));
		return;
	}
	if (BeginRequest(NSLOCTEXT("Rooms", "Creating", "방을 준비하고 있습니다…")))
	{
		// Public 방은 Master에 Dedicated 생성을 요청한다.
		RoomSubsystem->CreateRoom(Definition);
	}
}

void UDRRoomServiceWidget::CloseCreateRoom()
{
	if (bConnecting)
	{
		return;
	}
	CancelOwnedRequest();
	if (CreatePanel)
	{
		CreatePanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	bCreatePanelOpen = false;
	RefreshEnabledState();
	FocusRoomScreen();
}

void UDRRoomServiceWidget::HandlePrivateJoinClicked()
{
	if (bConnecting || bCreatePanelOpen || !ListenJoinPanel)
	{
		return;
	}
	// Master 응답 대기와 무관하게 기존 IP 직접 접속 창을 사용한다.
	CancelOwnedRequest();
	ListenJoinPanel->Show(this);
}

void UDRRoomServiceWidget::HandlePrivateJoinConfirmClicked()
{
	if (bPrivatePanelOpen && RoomCodeInput)
	{
		JoinRoomById(RoomCodeInput->GetText().ToString());
	}
}

void UDRRoomServiceWidget::HandlePrivateJoinCloseClicked()
{
	if (bConnecting)
	{
		return;
	}
	CancelOwnedRequest();
	bPrivatePanelOpen = false;
	if (PrivateJoinPanel)
	{
		PrivateJoinPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshEnabledState();
	FocusRoomScreen();
}

void UDRRoomServiceWidget::JoinRoomById(const FString& Id)
{
	if (bCreatePanelOpen)
	{
		return;
	}
	const FString Trimmed = Id.TrimStartAndEnd();
	if (!RoomServiceProtocol::IsIdentifier(Trimmed))
	{
		SetStatus(NSLOCTEXT("Rooms", "BadCode", "올바른 RoomId를 입력해 주세요."));
		return;
	}
	if (BeginRequest(NSLOCTEXT("Rooms", "Joining", "입장을 요청하고 있습니다…")))
	{
		RoomSubsystem->JoinRoom(Trimmed);
	}
}

void UDRRoomServiceWidget::HandleRoomListReceived(const TArray<FRoomServiceInfo>& Rooms)
{
	if (!bOwnsRequest)
	{
		return;
	}
	bOwnsRequest = false;
	bBusy = false;
	TArray<UObject*> Items;
	const auto* Settings = GetDefault<URoomServiceSettings>();
	for (const FRoomServiceInfo& Info : Rooms)
	{
		if (Info.bPrivate)
		{
			continue;
		}
		UDRRoomListItem* Item = NewObject<UDRRoomListItem>(this);
		Item->RoomInfo = Info;
		Item->RoomOwner = this;
		const FString* Package = Settings->Maps.Find(Info.MapId);
		if (Package && MapDefinitionTable)
		{
			for (const FName RowName : MapDefinitionTable->GetRowNames())
			{
				const auto* Row = MapDefinitionTable->FindRow<FDRTitleMapDefinition>(
					RowName, TEXT("Room list preview"));
				if (Row && Row->Map.ToSoftObjectPath().GetLongPackageName() == *Package)
				{
					Item->Preview = Row->PreviewImage.LoadSynchronous();
					break;
				}
			}
		}
		Items.Add(Item);
	}
	RoomListView->SetListItems(Items);
	SetStatus(Items.IsEmpty() ? NSLOCTEXT("Rooms", "Empty", "현재 공개된 방이 없습니다.")
		: FText::GetEmpty());
	RefreshEnabledState();
}

void UDRRoomServiceWidget::HandleConnectionReceived(const FRoomServiceConnection& Connection)
{
	if (!bOwnsRequest)
	{
		return;
	}
	bOwnsRequest = false;
	bConnecting = true;
	bBusy = true;
	LastRoomId = Connection.RoomId;
	SetStatus(NSLOCTEXT("Rooms", "Connecting", "서버에 접속하고 있습니다…"));
	RefreshEnabledState();
	if (!RoomSubsystem->ConnectToRoom(Connection))
	{
		HandleRequestFailed(TEXT("invalid_connection"));
	}
}

void UDRRoomServiceWidget::HandleRequestFailed(const FString& Error)
{
	if (!bOwnsRequest && !bConnecting)
	{
		return;
	}
	bOwnsRequest = false;
	bConnecting = false;
	bBusy = false;
	FText Message = NSLOCTEXT("Rooms", "Failed", "서버 요청에 실패했습니다. 다시 시도해 주세요.");
	if (Error == TEXT("room_not_joinable"))
	{
		Message = NSLOCTEXT("Rooms", "NoSeat", "방이 가득 찼거나 이미 시작되었습니다.");
	}
	else if (Error == TEXT("max_rooms") || Error == TEXT("no_game_port"))
	{
		Message = NSLOCTEXT("Rooms", "Capacity", "서버가 가득 찼습니다. 잠시 후 다시 시도해 주세요.");
	}
	else if (Error == TEXT("request_timeout") || Error == TEXT("master_request_failed"))
	{
		Message = NSLOCTEXT("Rooms", "Timeout", "서버 응답이 없습니다. 연결 상태를 확인해 주세요.");
	}
	else if (Error == TEXT("room_unavailable") || Error == TEXT("server_launch_failed"))
	{
		Message = NSLOCTEXT("Rooms", "StartFailed", "방을 준비하지 못했습니다. 다시 시도해 주세요.");
	}
	SetStatus(Message);
	RefreshEnabledState();
}

void UDRRoomServiceWidget::CancelOwnedRequest()
{
	if (bOwnsRequest && RoomSubsystem)
	{
		RoomSubsystem->CancelRequest();
	}
	bOwnsRequest = false;
	bBusy = false;
	SetStatus(FText::GetEmpty());
}

void UDRRoomServiceWidget::HandleNetworkFailure(UWorld* World, UNetDriver* Driver,
	ENetworkFailure::Type Type, const FString& Error)
{
	if (bConnecting && World && World->GetGameInstance() == GetGameInstance())
	{
		HandleRequestFailed(Error);
	}
}

void UDRRoomServiceWidget::HandleTravelFailure(UWorld* World,
	ETravelFailure::Type Type, const FString& Error)
{
	if (bConnecting && World && World->GetGameInstance() == GetGameInstance())
	{
		HandleRequestFailed(Error);
	}
}

void UDRRoomServiceWidget::ReleaseBindings()
{
	if (RoomSubsystem)
	{
		RoomSubsystem->OnRoomListReceived.RemoveDynamic(this, &ThisClass::HandleRoomListReceived);
		RoomSubsystem->OnConnectionReceived.RemoveDynamic(this, &ThisClass::HandleConnectionReceived);
		RoomSubsystem->OnRequestFailed.RemoveDynamic(this, &ThisClass::HandleRequestFailed);
		if (bOwnsRequest)
		{
			RoomSubsystem->CancelRequest();
		}
	}
	bOwnsRequest = false;
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	NetworkFailureHandle.Reset();
	TravelFailureHandle.Reset();
}

void UDRRoomServiceWidget::HandleExitClicked()
{
	if (bConnecting || bCreatePanelOpen || bPrivatePanelOpen)
	{
		return;
	}
	CancelOwnedRequest();
	if (ReturnWidget.IsValid())
	{
		if (TitleContent.IsValid())
		{
			TitleContent->SetVisibility(ESlateVisibility::Visible);
		}
		ReturnWidget->SetIsFocusable(true);
		if (APlayerController* Controller = GetOwningPlayer())
		{
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(ReturnWidget->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Controller->SetInputMode(Mode);
		}
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDRRoomServiceWidget::NativeDestruct()
{
	ReleaseBindings();
	if (CreatePanel)
	{
		CreatePanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	Super::NativeDestruct();
}
