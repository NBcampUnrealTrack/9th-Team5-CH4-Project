#include "DRCreateRoomWidget.h"

#include "DRRoomServiceWidget.h"
#include "DeepRaiders/UI/Title/DRTitleTextSettingRowWidget.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "RoomServiceSettings.h"

void UDRCreateRoomWidget::Open(UDRRoomServiceWidget* InOwner)
{
	RoomOwner = InOwner;
	SetVisibility(ESlateVisibility::Visible);
	Show();
	if (RoomTitleInput)
	{
		RoomTitleInput->SetKeyboardFocus();
	}
	else
	{
		SetIsFocusable(true);
		SetKeyboardFocus();
	}
}

void UDRCreateRoomWidget::SetFeedback(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
	OnFeedbackChanged(Message);
}

void UDRCreateRoomWidget::SetSubmitting(bool bSubmitting)
{
	if (RoomTitleInput)
	{
		RoomTitleInput->SetIsEnabled(!bSubmitting);
	}
	if (RoomTitleRow)
	{
		RoomTitleRow->SetIsEnabled(!bSubmitting);
	}
	PrivateCheckBox->SetIsEnabled(!bSubmitting);
	ComboBoxString_ChoiceMap->SetIsEnabled(!bSubmitting);
	CreateMap->SetIsEnabled(!bSubmitting && !GetSelectedPlayMap().IsNull()
		&& (RoomTitleInput || RoomTitleRow));
}

void UDRCreateRoomWidget::HandleCreateMapClicked()
{
	// 비밀 방은 선택한 맵을 listen으로 열며 Master 등록/맵 허용 목록을 거치지 않는다.
	if (PrivateCheckBox->IsChecked())
	{
		Super::HandleCreateMapClicked();
		return;
	}
	if (!RoomOwner.IsValid())
	{
		return;
	}
	FRoomServiceInfo Definition;
	const FText Title = RoomTitleInput ? RoomTitleInput->GetText()
		: RoomTitleRow ? RoomTitleRow->GetSettingText() : FText::GetEmpty();
	Definition.Title = Title.ToString().TrimStartAndEnd();
	Definition.bPrivate = PrivateCheckBox->IsChecked();
	Definition.MaxPlayers = MaxRoomPlayers;
	const FString Package = GetSelectedPlayMap().ToSoftObjectPath().GetLongPackageName();
	// 표시 이름이나 행 이름을 추측하지 않고 Master의 맵 허용 목록 키를 사용한다.
	for (const auto& Pair : GetDefault<URoomServiceSettings>()->Maps)
	{
		if (Pair.Value == Package)
		{
			Definition.MapId = Pair.Key;
			break;
		}
	}
	RoomOwner->SubmitCreateRoom(Definition);
}

void UDRCreateRoomWidget::HandleCloseChoiceMapClicked()
{
	if (RoomOwner.IsValid())
	{
		RoomOwner->CloseCreateRoom();
	}
}
