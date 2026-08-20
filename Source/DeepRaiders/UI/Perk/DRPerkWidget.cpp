#include "DRPerkWidget.h"

#include "Blueprint/WidgetTree.h"
#include "DeepRaiders/Perk/Components/DRPerkComponent.h"
#include "DeepRaiders/UI/Perk/DRPerkSlotWidget.h"

void UDRPerkWidget::InitializePerks(
	UDRPerkComponent* NewPerkComponent)
{
	// 슬롯 위젯은 코드에서 생성하지 않고 디자이너에 배치된 슬롯을 사용한다.
	CachePerkSlots();

	// 같은 컴포넌트가 다시 전달되면 이벤트를 중복 연결하지 않고 표시만 갱신한다.
	if (PerkComponent == NewPerkComponent)
	{
		RefreshPerks();
		return;
	}

	UnbindPerkComponent();
	PerkComponent = NewPerkComponent;

	if (IsValid(PerkComponent))
	{
		// 구매 또는 복제로 퍽 목록이 변경될 때 즉시 UI를 갱신한다.
		PerkComponent->OnPerksChanged.AddUniqueDynamic(
			this,
			&ThisClass::RefreshPerks);
	}

	RefreshPerks();
}

void UDRPerkWidget::NativeDestruct()
{
	UnbindPerkComponent();
	PerkSlots.Reset();
	Super::NativeDestruct();
}

void UDRPerkWidget::CachePerkSlots()
{
	if (!PerkSlots.IsEmpty() || !IsValid(WidgetTree))
	{
		return;
	}

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		if (UDRPerkSlotWidget* PerkSlot = Cast<UDRPerkSlotWidget>(Widget))
		{
			PerkSlots.Add(PerkSlot);
		}
	}
}

void UDRPerkWidget::RefreshPerks()
{
	// 컴포넌트가 아직 준비되지 않은 경우 모든 슬롯을 숨긴다.
	const int32 MaxPerkSlotCount = IsValid(PerkComponent)
		? PerkComponent->GetMaxPerkSlotCount()
		: 0;
	const TArray<FDRPerkEntry>* PerkEntries = IsValid(PerkComponent)
		? &PerkComponent->GetPerkEntries()
		: nullptr;

	for (int32 SlotIndex = 0;
		SlotIndex < PerkSlots.Num();
		++SlotIndex)
	{
		UDRPerkSlotWidget* PerkSlot = PerkSlots[SlotIndex].Get();
		if (!IsValid(PerkSlot))
		{
			continue;
		}

		const bool IsAvailableSlot = SlotIndex < MaxPerkSlotCount;
		// 최대 슬롯 수 안의 빈 슬롯은 유지하고 초과 슬롯만 숨긴다.
		PerkSlot->SetVisibility(
			IsAvailableSlot
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);

		// 구매된 퍽은 배열 순서대로 앞쪽 빈 슬롯부터 표시한다.
		const UDRPerkDefinition* PerkDefinition =
			IsAvailableSlot && PerkEntries
				&& PerkEntries->IsValidIndex(SlotIndex)
				? (*PerkEntries)[SlotIndex].PerkDefinition
				: nullptr;
		PerkSlot->SetPerkDefinition(PerkDefinition);
	}
}

void UDRPerkWidget::UnbindPerkComponent()
{
	if (IsValid(PerkComponent))
	{
		PerkComponent->OnPerksChanged.RemoveDynamic(
			this,
			&ThisClass::RefreshPerks);
	}

	PerkComponent = nullptr;
}
