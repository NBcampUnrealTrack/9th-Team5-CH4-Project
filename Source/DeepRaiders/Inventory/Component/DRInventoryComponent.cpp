// Fill out your copyright notice in the Description page of Project Settings.


#include "DRInventoryComponent.h"

#include "DeepRaiders/Item/DRItemDefinition.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UDRInventoryComponent::UDRInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicatedByDefault(true);
}

void UDRInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, MaxSlots);
	DOREPLIFETIME(ThisClass, Entries);
}

bool UDRInventoryComponent::TryAddItem(UDRItemDefinition* Definition, int32 Quantity)
{
	UE_LOG(LogTemp, Log, TEXT("[%s] TryAddItem start, Quantity : %d"), *GetName(), GetItemCount(Definition));
	
	if (!HasInventoryAuthority()
		|| !CanAddItem(Definition, Quantity))
	{
		return false;
	}
	
	const int32 MaxStackSize = GetMaxStackSize(Definition);
	
	int32 RemainingQuantity = Quantity;
	
	// 기존 스택 먼저 채운다.
	for (FDRInventoryEntry& Entry : Entries)
	{
		if (RemainingQuantity <= 0)
		{
			break;
		}
		
		if (Entry.Definition != Definition)
		{
			continue;
		}
		
		const int32 FreeQuantity = FMath::Max(0, MaxStackSize - Entry.Quantity);
		const int32 AddedQuantity = FMath::Min(RemainingQuantity, FreeQuantity);
		
		if (AddedQuantity <= 0)
		{
			continue;
		}
		
		Entry.Quantity += AddedQuantity;
		RemainingQuantity -= AddedQuantity;		
	}
	
	// 남은 수량은 새로운 스택을 만들어 넣는다.
	while (RemainingQuantity > 0)
	{
		const int32 NewStackQuantity = FMath::Min(RemainingQuantity, MaxStackSize);
		
		FDRInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.EntryId = FGuid::NewGuid();
		NewEntry.Definition = Definition;
		NewEntry.Quantity = NewStackQuantity;
		
		RemainingQuantity -= NewStackQuantity;
		
		CachedEntryId = NewEntry.EntryId;
	}
	
	HandleInventoryChangedOnServer();
	
	
	UE_LOG(LogTemp, Log, TEXT("[%s] TryAddItem End, Quantity : %d"), *GetName(), GetItemCount(Definition));
	return true;
}

bool UDRInventoryComponent::TryRemoveFromEntry(FGuid EntryId, int32 Quantity)
{
	UE_LOG(LogTemp, Log, TEXT("[%s] TryRemoveFromEntry start"), *GetName());
	
	if (!HasInventoryAuthority()
		|| !EntryId.IsValid()
		|| Quantity <= 0)
	{
		return false;
	}
	
	const int32 EntryIndex = Entries.IndexOfByPredicate(
		[&EntryId](const FDRInventoryEntry& Entry)
	{
			return Entry.EntryId == EntryId;
	});
	
	if (!Entries.IsValidIndex(EntryIndex))
	{
		return false;
	}
	
	FDRInventoryEntry& Entry = Entries[EntryIndex];
	
	if (Entry.Quantity < Quantity)
	{
		// 수량만큼 가지고 있지 않다면 그냥 실패
		return false;
	}
	
	if (Entry.Quantity == Quantity)
	{
		Entries.RemoveAt(EntryIndex);
	}
	else
	{
		Entry.Quantity -= Quantity;
		UE_LOG(LogTemp, Log, TEXT("[%s] TryRemoveFromEntry process, Quantity : %d"), *GetName(), Entry.Quantity);
	}
	
	HandleInventoryChangedOnServer();
	
	UE_LOG(LogTemp, Log, TEXT("[%s] TryRemoveFromEntry End"), *GetName());
	return true;
}

bool UDRInventoryComponent::TryRemoveItemByDefinition(UDRItemDefinition* Definition, int32 Quantity)
{
	UE_LOG(LogTemp, Log, TEXT("[%s] TryRemoveItemByDefinition start, Quantity : %d"), *GetName(), GetItemCount(Definition));
	
	if (!HasInventoryAuthority()
	|| !IsValid(Definition)
	|| Quantity <= 0)
	{
		return false;
	}
	
	if (GetItemCount(Definition) < Quantity)
	{
		// 수량만큼 가지고 있지 않다면 그냥 실패
		return false;
	}
	
	int32 RemainingQuantity = Quantity;
	
	// 배열 뒤에서부터 제거
	for (int32 Index = Entries.Num() -1; Index >= 0 && RemainingQuantity > 0 ; --Index)
	{
		FDRInventoryEntry& Entry = Entries[Index];
		
		if (Entry.Definition != Definition)
		{
			continue;
		}
		
		if (Entry.Quantity <= RemainingQuantity)
		{
			RemainingQuantity -= Entry.Quantity;
			Entries.RemoveAt(Index);
		}
		else
		{
			Entry.Quantity -= RemainingQuantity;
			RemainingQuantity = 0;
		}
	}
	
	HandleInventoryChangedOnServer();
	
	UE_LOG(LogTemp, Log, TEXT("[%s] TryRemoveItemByDefinition start, Quantity : %d"), *GetName(), GetItemCount(Definition));
	return true;
}

bool UDRInventoryComponent::FindEntry(FGuid EntryId, FDRInventoryEntry& OutEntry) const
{
	UE_LOG(LogTemp, Log, TEXT("[%s] FindEntry Start, Quantity : %d"), *GetName(), OutEntry.Quantity);
	
	if (!EntryId.IsValid())
	{
		OutEntry = FDRInventoryEntry();
		return false;
	}
	
	const FDRInventoryEntry* FoundEntry = Entries.FindByPredicate(
		[&EntryId](const FDRInventoryEntry& Entry)
		{
			return Entry.EntryId == EntryId;
		});
	
	if (FoundEntry == nullptr)
	{
		OutEntry = FDRInventoryEntry();
		return false;
	}
	
	OutEntry = *FoundEntry;
	
	UE_LOG(LogTemp, Log, TEXT("[%s] FindEntry End, Quantity : %d"), *GetName(), OutEntry.Quantity);
	
	return true;
}

bool UDRInventoryComponent::CanAddItem(UDRItemDefinition* Definition, int32 Quantity) const
{
	return IsValid(Definition) 
	&& Quantity > 0 
	&& GetAddableQuantity(Definition) >= Quantity;
}

int32 UDRInventoryComponent::GetAddableQuantity(UDRItemDefinition* Definition) const
{
	if (!IsValid(Definition))
	{
		return 0;
	}
	
	const int32 MaxStackSize = GetMaxStackSize(Definition);
	int32 AvailableQuantity = 0;
	
	// 기존 스택의 남은 공간 계산
	for (const FDRInventoryEntry& Entry : Entries)
	{
		if (Entry.Definition != Definition)
		{
			continue;
		}
		
		AvailableQuantity += FMath::Max(0, MaxStackSize - Entry.Quantity);
	}
	
	// 새 스택을 만들 수 있는지 계산
	const int32 ClampedMaxSlots = FMath::Max(1, MaxSlots);
	const int32 FreeSlots = FMath::Max(0, ClampedMaxSlots - Entries.Num());
	
	AvailableQuantity += FreeSlots * MaxStackSize;
	
	return AvailableQuantity;
}

int32 UDRInventoryComponent::GetItemCount(UDRItemDefinition* Definition) const
{
	if (!IsValid(Definition))
	{
		return 0;
	}
	
	int32 TotalQuantity = 0;
	
	for (const FDRInventoryEntry& Entry : Entries)
	{
		if (Entry.Definition == Definition)
		{
			TotalQuantity += Entry. Quantity;
		}
	}
	
	return TotalQuantity;
}

void UDRInventoryComponent::OnRep_Entries()
{
	BroadcastInventoryChanged();
}

void UDRInventoryComponent::HandleInventoryChangedOnServer()
{
	BroadcastInventoryChanged();
	
	AActor* OwnerActor = GetOwner();
	
	if (!IsValid(OwnerActor)
		|| !OwnerActor->GetIsReplicated())
	{
		return;
	}
	
	// 휴면 상태 인벤토리 깨우기
	OwnerActor->FlushNetDormancy();
	OwnerActor->ForceNetUpdate();	
}

bool UDRInventoryComponent::HasInventoryAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	
	return IsValid(OwnerActor) 
		&& OwnerActor->HasAuthority();
}

int32 UDRInventoryComponent::GetMaxStackSize(const UDRItemDefinition* Definition) const
{
	if (!IsValid(Definition))
	{
		return 1;
	}
	
	return FMath::Max(1, Definition->MaxStackSize);
}

void UDRInventoryComponent::BroadcastInventoryChanged()
{
	OnInventoryChangedDelegate.Broadcast();
}
