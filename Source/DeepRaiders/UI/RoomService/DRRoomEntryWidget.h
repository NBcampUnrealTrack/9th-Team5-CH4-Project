#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "RoomServiceTypes.h"
#include "DRRoomEntryWidget.generated.h"

class UDRRoomServiceWidget;
class UImage;
class UTextBlock;
class UTexture2D;

// ListView가 소유하는 데이터이며, 재활용되는 항목 위젯과 수명을 분리한다.
UCLASS(BlueprintType)
class DEEPRAIDERS_API UDRRoomListItem : public UObject
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnChanged;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms")
	FRoomServiceInfo RoomInfo;
	UPROPERTY()
	TObjectPtr<UTexture2D> Preview;
	TWeakObjectPtr<UDRRoomServiceWidget> RoomOwner;
};

UCLASS()
class DEEPRAIDERS_API UDRRoomEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Rooms")
	void HandleJoinClicked();

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;

	UPROPERTY(BlueprintReadOnly, Category = "Rooms", meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomId;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms", meta = (BindWidget))
	TObjectPtr<UImage> RoomImage;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms", meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomName;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms", meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomJoinCount;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms", meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomState;
	UPROPERTY(BlueprintReadOnly, Category = "Rooms", meta = (BindWidget))
	TObjectPtr<UWidget> Join;

private:
	void RefreshRoom();
	UPROPERTY(Transient)
	TObjectPtr<UDRRoomListItem> CurrentItem;
};
