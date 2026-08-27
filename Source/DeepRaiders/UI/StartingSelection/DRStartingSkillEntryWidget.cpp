#include "DRStartingSkillEntryWidget.h"

#include "DeepRaiders/UI/ViewModel/DRStartingSkillViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UDRStartingSkillEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	UDRStartingSkillEntryViewModel* EntryViewModel =
		Cast<UDRStartingSkillEntryViewModel>(ListItemObject);
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	const bool IsAssigned = IsValid(EntryViewModel)
		&& IsValid(View)
		&& View->SetViewModel(ViewModelName, EntryViewModel);

	if (!IsAssigned)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Starting skill entry ViewModel '%s' could not be assigned to %s"),
			*ViewModelName.ToString(),
			*GetName());
	}
}
