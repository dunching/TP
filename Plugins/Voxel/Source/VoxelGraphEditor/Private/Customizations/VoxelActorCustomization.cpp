// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelActor.h"
#include "Customizations/VoxelParameterContainerDetails.h"

VOXEL_CUSTOMIZE_CLASS(AVoxelActor)(IDetailLayoutBuilder& DetailLayout)
{
	VOXEL_FUNCTION_COUNTER_LLM();

	FVoxelEditorUtilities::EnableRealtime();

	IDetailCategoryBuilder& ActorCategory = DetailLayout.EditCategory(STATIC_FNAME("Actor"));

	TArray<TSharedRef<IPropertyHandle>> ActorProperties;
	ActorCategory.GetDefaultProperties(ActorProperties);

	for (const TSharedRef<IPropertyHandle>& ActorPropertyHandle : ActorProperties)
	{
		if (ActorPropertyHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_STATIC(AActor, Tags))
		{
			IDetailCategoryBuilder& DefaultCategory = DetailLayout.EditCategory(STATIC_FNAME("Default"));
			DefaultCategory.AddProperty(ActorPropertyHandle);
			break;
		}
	}

	DetailLayout.HideCategory(STATIC_FNAME("Actor"));

	TArray<TWeakObjectPtr<UObject>> WeakObjects;
	DetailLayout.GetObjectsBeingCustomized(WeakObjects);

	TArray<UObject*> Objects;
	for (const TWeakObjectPtr<UObject>& WeakObject : WeakObjects)
	{
		Objects.Add(WeakObject.Get());
	}

	IDetailPropertyRow* Row = DetailLayout.EditCategory("").AddExternalObjectProperty(
		Objects,
		GET_MEMBER_NAME_STATIC(AVoxelActor, ParameterContainer),
		EPropertyLocation::Default,
		FAddPropertyParams().ForceShowProperty());

	if (!ensure(Row))
	{
		return;
	}

	Row->Visibility(EVisibility::Collapsed);

	const TSharedPtr<IPropertyHandle> PropertyHandle = Row->GetPropertyHandle();
	if (!ensure(PropertyHandle))
	{
		return;
	}

	KeepAlive(FVoxelParameterContainerDetails::Create(DetailLayout, PropertyHandle.ToSharedRef()));
}