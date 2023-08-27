// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelBufferInterface.generated.h"

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelBufferInterface : public FVoxelVirtualStruct
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	virtual int32 Num_Slow() const VOXEL_PURE_VIRTUAL({});
	virtual bool IsValid_Slow() const VOXEL_PURE_VIRTUAL({});
};