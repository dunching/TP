// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelSurface.h"
#include "VoxelGetSculptVolumeSurfaceNode.generated.h"

USTRUCT(Category = "Sculpt")
struct VOXELGRAPHCORE_API FVoxelNode_GetSculptVolumeSurface : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_OUTPUT_PIN(FVoxelSurface, Surface);
};

USTRUCT(meta = (Internal))
struct VOXELGRAPHCORE_API FVoxelNode_GetSculptVolumeSurface_Distance : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_OUTPUT_PIN(FVoxelFloatBuffer, Distance);
};

USTRUCT(meta = (Internal))
struct VOXELGRAPHCORE_API FVoxelNode_GetSculptVolumeSurface_Material : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_OUTPUT_PIN(FVoxelSurfaceMaterial, Material);
};