// ReSharper disable CppMemberFunctionMayBeConst
#include "HostOrJoinWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "DeepRaiders/Core/Subsystem/DRSessionSubsystem.h"

bool UHostOrJoinWidget::Initialize()
{
	if (!Super::Initialize()) return false;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDRSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UDRSessionSubsystem>())
		{
			SessionSubsystem->OnCreateSessionComplete.AddDynamic(this, &UHostOrJoinWidget::OnCreateSessionComplete);
			SessionSubsystem->OnJoinSessionComplete.AddDynamic(this, &UHostOrJoinWidget::OnJoinSessionComplete);
		}
	}

	if (Btn_Host) Btn_Host->OnClicked.AddDynamic(this, &UHostOrJoinWidget::OnHostButtonClicked);
	if (Btn_Join) Btn_Join->OnClicked.AddDynamic(this, &UHostOrJoinWidget::OnJoinButtonClicked);
	if (ETB_IPAddress) ETB_IPAddress->SetHintText(FText::FromString(TEXT("서버 IP 주소를 입력하세요...")));

	return true;
}

void UHostOrJoinWidget::OnHostButtonClicked()
{
	if (Btn_Host) Btn_Host->SetIsEnabled(false);

	if (UDRSessionSubsystem* SessionSubsystem = GetGameInstance()->GetSubsystem<UDRSessionSubsystem>())
	{
		SessionSubsystem->CreateSession(4, FName("FreeForAll"), FName(MapPath));
	}
}

void UHostOrJoinWidget::OnJoinButtonClicked()
{
	if (Btn_Join) Btn_Join->SetIsEnabled(false);

	UDRSessionSubsystem* SessionSubsystem = GetGameInstance()->GetSubsystem<UDRSessionSubsystem>();
	if (!SessionSubsystem)
		return;

	if (ETB_IPAddress)
	{
		FString TargetIP = ETB_IPAddress->GetText().ToString();

		TargetIP = TargetIP.TrimStartAndEnd();
		if (TargetIP.IsEmpty()) return;

		SessionSubsystem->JoinSession(TargetIP);
	}
	else
	{
		SessionSubsystem->FindAndJoinSession();
	}
}

void UHostOrJoinWidget::OnCreateSessionComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		if (Btn_Host) Btn_Host->SetIsEnabled(true);
		UE_LOG(LogTemp, Warning, TEXT("방 생성 실패!"));
	}
}

void UHostOrJoinWidget::OnJoinSessionComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		if (Btn_Join) Btn_Join->SetIsEnabled(true);
		UE_LOG(LogTemp, Warning, TEXT("방 입장 실패!"));
	}
}
