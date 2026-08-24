#include "DRStartingWeaponEntryWidget.h"

#include "DeepRaiders/UI/ViewModel/DRStartingWeaponViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRStartingWeaponEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	UDRStartingWeaponEntryViewModel* EntryViewModel =
		Cast<UDRStartingWeaponEntryViewModel>(ListItemObject);
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	const bool IsAssigned = IsValid(EntryViewModel)
		&& IsValid(View)
		&& View->SetViewModel(ViewModelName, EntryViewModel);

	if (!IsAssigned)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Starting weapon entry ViewModel '%s' could not be assigned to %s"),
			*ViewModelName.ToString(),
			*GetName());
	}
}
