// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelSurface.h"
#include "VoxelSurfaceNodes.generated.h"

USTRUCT(Category = "Surface")
struct VOXELGRAPHCORE_API FVoxelNode_GetSurfaceDistance : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelSurface, Surface, nullptr);
	VOXEL_OUTPUT_PIN(FVoxelFloatBuffer, Distance);
};

USTRUCT(Category = "Surface")
struct VOXELGRAPHCORE_API FVoxelNode_GetSurfaceMaterial : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelSurface, Surface, nullptr);
	VOXEL_OUTPUT_PIN(FVoxelSurfaceMaterial, Material);
};

USTRUCT(Category = "Surface")
struct VOXELGRAPHCORE_API FVoxelNode_MakeHeightSurface : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Height, nullptr);
	VOXEL_OUTPUT_PIN(FVoxelSurface, Surface);
};

USTRUCT(Category = "Surface", meta = (Keywords = "float surface"))
struct VOXELGRAPHCORE_API FVoxelNode_MakeVolumetricSurface : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Distance, nullptr);
	// If not set will be infinite
	VOXEL_INPUT_PIN(FVoxelBox, Bounds, nullptr, OptionalPin, AdvancedDisplay);
	VOXEL_OUTPUT_PIN(FVoxelSurface, Surface);
};

USTRUCT(Category = "Surface")
struct VOXELGRAPHCORE_API FVoxelNode_SetSurfaceMaterial : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelSurface, Surface, nullptr);
	VOXEL_INPUT_PIN(FVoxelSurfaceMaterial, Material, nullptr);
	VOXEL_OUTPUT_PIN(FVoxelSurface, NewSurface);
};

USTRUCT(Category = "Surface")
struct VOXELGRAPHCORE_API FVoxelNode_MakeSurfaceMaterial : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelMaterialDefinitionBuffer, Material, nullptr);
	VOXEL_OUTPUT_PIN(FVoxelSurfaceMaterial, Result);
};