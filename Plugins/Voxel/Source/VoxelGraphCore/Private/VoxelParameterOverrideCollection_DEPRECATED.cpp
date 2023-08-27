// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelParameterOverrideCollection_DEPRECATED.h"
#include "VoxelParameterContainer.h"

void FVoxelParameterOverrideCollection_DEPRECATED::MigrateTo(UVoxelParameterContainer& ParameterContainer)
{
	for (const FVoxelParameterOverride_DEPRECATED& Parameter : Parameters)
	{
		FVoxelParameterPath Path;
		Path.Guids.Add(Parameter.Parameter.Guid);

		FVoxelParameterValueOverride ValueOverride;
		ValueOverride.bEnable = Parameter.bEnable;
		ValueOverride.CachedName = Parameter.Parameter.Name;
		ValueOverride.CachedCategory = FName(Parameter.Parameter.Category);
		ValueOverride.Value = Parameter.ValueOverride;

		ensure(!ParameterContainer.ValueOverrides.Contains(Path));
		ParameterContainer.ValueOverrides.Add(Path, ValueOverride);
	}

	ParameterContainer.Fixup();

	Parameters = {};
	Categories = {};
}