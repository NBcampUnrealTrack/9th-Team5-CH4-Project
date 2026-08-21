// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRInventoryUIComponent.generated.h"

class ADRPlayerController;
class ADRStorage;
class UDRInventoryWidget;
class UDRInventoryScreenWidget;
class UDRUIManagerSubsystem;
class APlayerState;

enum class EDRInventoryUIState : uint8
{
	Closed,
	PlayerOnly,
	PlayerAndStorage,
};

// 현재 이 클래스는 사라진 DRPlayerController 코드를 상당수 의존하고 있었기에
// 사용이 불가능한 클래스입니다.

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

	/** 열린 인벤토리 화면을 BP에서 안전하게 닫는다. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void CloseInventory();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	void ShowPlayerInventory();
	void HidePlayerInventory();
	void SetInventoryOpenTag(bool bIsOpen) const;
	
	void ShowStorageInventory(ADRStorage* Storage);
	void HideStorageInventory();
	
	// 열린 인벤토리를 닫고 상태를 초기화
	void CloseInventoryScreen();
	
	// 창고와의 거리가 멀어지면 자동으로 UI가 닫히도록 타이머로 체크
	void StartStorageDistanceCheck();
	void StopStorageDistanceCheck();
	void CheckStorageDistance();
	
	UFUNCTION()
	void HandleCurrentStorageChanged(ADRStorage* NewStorage);
	
	UFUNCTION()
	void HandlePlayerEntryClicked(FGuid InstanceId);
	
	UFUNCTION()
	void HandleStorageEntryClicked(FGuid InstanceId);
	
	UFUNCTION()
	void HandleCloseRequested();
	
	UFUNCTION()
	void HandleStorageOwnerChanged(AActor* PreviousOwner, AActor* NewOwner);
	
private:
	UPROPERTY(Transient)
	TObjectPtr<ADRPlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UDRUIManagerSubsystem> UIManager;
	
	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryScreenWidget> PlayerInventoryWidget;
	
	UPROPERTY(Transient)
	TObjectPtr<UDRInventoryWidget> StorageInventoryWidget;
	
	EDRInventoryUIState UIState = EDRInventoryUIState::Closed;
	FTimerHandle StorageDistanceTimerHandle;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<ADRStorage> CurrentStorage;
};
