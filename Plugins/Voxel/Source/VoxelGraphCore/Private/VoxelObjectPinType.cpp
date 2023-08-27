// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelObjectPinType.h"

const TVoxelMap<const UScriptStruct*, const FVoxelObjectPinType*>& FVoxelObjectPinType::StructToPinType()
{
	static TVoxelMap<const UScriptStruct*, const FVoxelObjectPinType*> Map;
	if (Map.Num() == 0)
	{
		VOXEL_FUNCTION_COUNTER();

		for (UScriptStruct* StructIt : GetDerivedStructs<FVoxelObjectPinType>())
		{
			TVoxelInstancedStruct<FVoxelObjectPinType> Instance(StructIt);
			const UScriptStruct* InstanceStruct = Instance->GetStruct();

			ensure(!Map.Contains(InstanceStruct));
			Map.Add(InstanceStruct, Instance.Release());
		}
	}
	return Map;
}