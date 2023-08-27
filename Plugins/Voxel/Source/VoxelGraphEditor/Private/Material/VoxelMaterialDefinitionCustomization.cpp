// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "Material/VoxelMaterialDefinition.h"

VOXEL_CUSTOMIZE_CLASS(UVoxelMaterialDefinition)(IDetailLayoutBuilder& DetailLayout)
{
	VOXEL_FUNCTION_COUNTER_LLM();

	DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(UVoxelMaterialDefinition, Parameters))->MarkHiddenByCustomization();
	DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(UVoxelMaterialDefinition, GuidToParameterData))->MarkHiddenByCustomization();
}