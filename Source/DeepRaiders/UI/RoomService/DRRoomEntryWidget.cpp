#include "DRRoomEntryWidget.h"

#include "DRRoomServiceWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UDRRoomEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	CurrentItem = Cast<UDRRoomListItem>(ListItemObject);
	const FRoomServiceInfo Info = CurrentItem ? CurrentItem->RoomInfo : FRoomServiceInfo();
	RoomId->SetText(FText::FromString(Info.RoomId));
	RoomName->SetText(FText::FromString(Info.Title));
	RoomJoinCount->SetText(FText::Format(NSLOCTEXT("Rooms", "PlayerCount", "{0} / {1}"),
		FText::AsNumber(Info.CurrentPlayers), FText::AsNumber(Info.MaxPlayers)));
	FText StateText = NSLOCTEXT("Rooms", "Unavailable", "입장 불가");
	if (Info.State == TEXT("Waiting"))
	{
		StateText = NSLOCTEXT("Rooms", "Waiting", "대기 중");
	}
	else if (Info.State == TEXT("Playing"))
	{
		StateText = NSLOCTEXT("Rooms", "Playing", "진행 중");
	}
	else if (Info.State == TEXT("Ending"))
	{
		StateText = NSLOCTEXT("Rooms", "Ending", "종료 중");
	}
	RoomState->SetText(StateText);
	RoomImage->SetBrushFromTexture(CurrentItem ? CurrentItem->Preview.Get() : nullptr);
	Join->SetIsEnabled(CurrentItem && Info.State == TEXT("Waiting")
		&& Info.CurrentPlayers < Info.MaxPlayers);
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
}

void UDRRoomEntryWidget::NativeOnEntryReleased()
{
	CurrentItem = nullptr;
	Join->SetIsEnabled(false);
	IUserObjectListEntry::NativeOnEntryReleased();
}

void UDRRoomEntryWidget::HandleJoinClicked()
{
	// 행 선택은 접속하지 않는다. 입장 버튼만 이 함수를 호출한다.
	if (CurrentItem && CurrentItem->RoomOwner.IsValid())
	{
		CurrentItem->RoomOwner->JoinRoomById(CurrentItem->RoomInfo.RoomId);
	}
}
