// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRInventoryUIComponent.generated.h"

class ADRPlayerController;
class ADRStorage;
class UDRInventoryWidget;

enum class EDRInventoryUIState : uint8
{
	Closed,
	PlayerOnly,
	PlayerAndStorage,
};

enum class EDRInventoryInputMode : uint8
{
	GameOnly,
	GameAndUI
};

// 창고 인벤토리를 보여줄 UI와 플레이어 인벤토리 UI를 모두 관리
// 필요 시 추후 변경 필요
UCLASS()
class DEEPRAIDERS_API UDRInventoryUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDRInventoryUIComponent();
	
	// 키 입력으로 Player Inventory 표시 상태 전환
	void TogglePlayerInventory();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	void ShowPlayerInventory();
	void HidePlayerInventory();
	
	void ShowStorageInventory(ADRStorage* Storage);
	void HideStorageInventory();
	
	// 호출 시 UIState 및 InputMode 초기화
	void CloseInventoryScreen();
	void ApplyInputMode(EDRInventoryInputMode InputMode);
	
	// 창고와의 거리가 멀어지면 자동으로 UI가 닫히도록 타이머로 체크
	void StartStorageDistanceCheck();
	void StopStorageDistanceCheck();
	void CheckStorageDistance();
	
	UFUNCTION()
	void HandleCurrentStorageChanged(ADRStorage* NewStorage);
	
	UFUNCTION()
	void HandlePlayerEntryClicked(FGuid EntryId);
	
	UFUNCTION()
	void HandleStorageEntryClicked(FGuid EntryId);
	
	UFUNCTION()
	void HandleCloseRequested();
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UDRInventoryWidget> PlayerInventoryWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UDRInventoryWidget> StorageInventoryWidgetClass;
	
	// 현재 접근 중인 창고와의 거리 체크 간격
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI", meta = (ClampMin = "0.05", Units = "s"))
	float StorageDistanceCheckInterval = 0.2f;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerController> PlayerController;
	
	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryWidget> PlayerInventoryWidget;
	
	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryWidget> StorageInventoryWidget;
	
	EDRInventoryUIState UIState = EDRInventoryUIState::Closed;
	FTimerHandle StorageDistanceTimerHandle;
};
