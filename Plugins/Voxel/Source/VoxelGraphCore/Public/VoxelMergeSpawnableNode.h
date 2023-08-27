// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelSpawnable.h"
#include "VoxelMergeSpawnableNode.generated.h"

USTRUCT(Category = "Spawnable")
struct VOXELGRAPHCORE_API FVoxelNode_MergeSpawnable : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN_ARRAY(FVoxelSpawnable, Input, nullptr, 1);
	VOXEL_OUTPUT_PIN(FVoxelSpawnable, Out);
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelMergedSpawnable : public FVoxelSpawnable
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	// Set to ensure we don't create the same spawnable twice
	TVoxelSet<TSharedPtr<FVoxelSpawnable>> Spawnables;

	//~ Begin FVoxelSpawnable Interface
	virtual void Create_AnyThread() override;
	virtual void Destroy_AnyThread() override;
	virtual FVoxelOptionalBox GetBounds() const override;
	//~ End FVoxelSpawnable Interface
};