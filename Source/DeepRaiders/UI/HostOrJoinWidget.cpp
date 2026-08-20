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
			SessionSubsystem->OnJoinSessionComplete.AddDynamic(this, &UHostOrJoinWidget::OnJoinSessionComplete);
		}
	}

	if (Btn_Host) Btn_Host->SetIsEnabled(false);
	if (Btn_Join) Btn_Join->OnClicked.AddDynamic(this, &UHostOrJoinWidget::OnJoinButtonClicked);
	if (ETB_IPAddress) ETB_IPAddress->SetHintText(FText::FromString(TEXT("서버 IP:Port 입력")));

	return true;
}

void UHostOrJoinWidget::OnJoinButtonClicked()
{
	if (Btn_Join) Btn_Join->SetIsEnabled(false);

	UDRSessionSubsystem* SessionSubsystem = GetGameInstance()->GetSubsystem<UDRSessionSubsystem>();
	if (!SessionSubsystem)
	{
		if (Btn_Join) Btn_Join->SetIsEnabled(true);
		return;
	}

	if (ETB_IPAddress)
	{
		FString TargetIP = ETB_IPAddress->GetText().ToString();

		TargetIP = TargetIP.TrimStartAndEnd();
		if (TargetIP.IsEmpty())
		{
			if (Btn_Join) Btn_Join->SetIsEnabled(true);
			UE_LOG(LogTemp, Warning, TEXT("서버 주소를 입력하세요."));
			return;
		}

		SessionSubsystem->JoinServer(TargetIP);
	}
	else
	{
		if (Btn_Join) Btn_Join->SetIsEnabled(true);
		UE_LOG(LogTemp, Warning, TEXT("IP 주소 입력 위젯이 없습니다."));
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
