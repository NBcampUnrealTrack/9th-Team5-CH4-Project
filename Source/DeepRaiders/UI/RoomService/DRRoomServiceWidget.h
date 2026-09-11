#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/EngineBaseTypes.h"
#include "RoomServiceTypes.h"
#include "DRRoomServiceWidget.generated.h"

class UDataTable;
class UDRCreateRoomWidget;
class UDRRoomListItem;
class UDRTitleJoinWidget;
class UEditableTextBox;
class UListView;
class URoomServiceClientSubsystem;
class UTextBlock;
class UNetDriver;

UCLASS()
class DEEPRAIDERS_API UDRRoomServiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(UUserWidget* InReturnWidget, UWidget* InTitleContent,
		UDRCreateRoomWidget* InCreatePanel, UDRTitleJoinWidget* InListenJoinPanel);
	void SubmitCreateRoom(const FRoomServiceInfo& Definition);
	void CloseCreateRoom();

	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void RefreshRooms();
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void HandleQuickMatchClicked();
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void HandleCreateRoomClicked();
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void HandlePrivateJoinClicked();
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void HandlePrivateJoinConfirmClicked();
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void HandlePrivateJoinCloseClicked();
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void HandleExitClicked();
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void JoinRoomById(const FString& Id);

	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	bool bBusy = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FText StatusMessage;
	// 생성/입장 시 받은 전체 RoomId를 복사·초대 UI에서 사용할 수 있다.
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FString LastRoomId;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rooms")
	void OnStatusChanged(const FText& Message);
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Rooms", meta = (BindWidget))
	TObjectPtr<UListView> RoomListView;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> QuickMatch;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CreateRoom;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PrivateJoin;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Exit;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PrivateJoinPanel;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RoomCodeInput;
	UPROPERTY(EditDefaultsOnly, Category = "Rooms")
	TObjectPtr<UDataTable> MapDefinitionTable;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rooms")
	ERoomQuickMatchMode QuickMatchMode = ERoomQuickMatchMode::AnyMap;
	// AnyMap에서는 방이 없을 때 생성할 맵으로만 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rooms")
	FString QuickMatchMapId = TEXT("SamplePlayMap");

private:
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UDRRoomListItem>> RoomItems;
	UFUNCTION()
	void HandleRoomDelta(const TArray<FRoomServiceInfo>& Changed,
		const TArray<FString>& Removed, bool bReset);
	UPROPERTY(Transient)
	TObjectPtr<UDRTitleJoinWidget> ListenJoinPanel;
	UPROPERTY(Transient)
	TObjectPtr<URoomServiceClientSubsystem> RoomSubsystem;
	UPROPERTY(Transient)
	TObjectPtr<UDRCreateRoomWidget> CreatePanel;
	TWeakObjectPtr<UUserWidget> ReturnWidget;
	TWeakObjectPtr<UWidget> TitleContent;
	bool bOwnsRequest = false;
	bool bConnecting = false;
	bool bPrivatePanelOpen = false;
	bool bCreatePanelOpen = false;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;

	bool BeginRequest(const FText& Message);
	void SetStatus(const FText& Message);
	void RefreshEnabledState();
	void ReleaseBindings();
	void CancelOwnedRequest();
	void FocusRoomScreen();

	UFUNCTION()
	void HandleRoomListReceived(const TArray<FRoomServiceInfo>& Rooms);
	UFUNCTION()
	void HandleConnectionReceived(const FRoomServiceConnection& Connection);
	UFUNCTION()
	void HandleRequestFailed(const FString& Error);
	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver,
		ENetworkFailure::Type Type, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
};
