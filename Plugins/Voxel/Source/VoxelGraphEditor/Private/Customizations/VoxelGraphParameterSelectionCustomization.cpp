// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphParameterSelectionCustomization.h"
#include "VoxelGraph.h"

void FVoxelGraphParameterSelectionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = DetailLayout.GetSelectedObjects();
	if (SelectedObjects.Num() != 1)
	{
		return;
	}

	DetailLayout.HideCategory("Config");

	const TSharedRef<IPropertyHandle> ParametersHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(UVoxelGraph, Parameters), UVoxelGraph::StaticClass());
	DetailLayout.HideProperty(ParametersHandle);

	uint32 ParametersCount = 0;
	ParametersHandle->GetNumChildren(ParametersCount);

	TSharedPtr<IPropertyHandle> ParameterHandle;
	for (uint32 Index = 0; Index < ParametersCount; Index++)
	{
		const TSharedPtr<IPropertyHandle> ChildParameterHandle = ParametersHandle->GetChildHandle(Index);

		FVoxelGraphParameter ChildParameter = FVoxelEditorUtilities::GetStructPropertyValue<FVoxelGraphParameter>(ChildParameterHandle);
		if (ChildParameter.Guid != TargetParameterGuid)
		{
			continue;
		}

		ensure(!ParameterHandle);
		ParameterHandle = ChildParameterHandle;
	}

	if (!ParameterHandle)
	{
		return;
	}

	IDetailCategoryBuilder& ParameterCategory = DetailLayout.EditCategory("Parameter", INVTEXT("Parameter"));
	ParameterCategory.AddProperty(ParameterHandle);
}