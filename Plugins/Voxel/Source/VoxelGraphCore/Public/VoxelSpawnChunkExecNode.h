// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelExecNode.h"
#include "VoxelSpawnable.h"
#include "VoxelSpawnChunkExecNode.generated.h"

// Spawn a single chunk
USTRUCT(DisplayName = "Spawn Chunk")
struct VOXELGRAPHCORE_API FVoxelSpawnChunkExecNode : public FVoxelExecNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelSpawnable, Spawnable, nullptr, VirtualPin);
	// Value to be passed through GetSpawnableBounds
	VOXEL_INPUT_PIN(FVoxelBox, Bounds, nullptr, VirtualPin, AdvancedDisplay);

	virtual TVoxelUniquePtr<FVoxelExecNodeRuntime> CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const override;
};

class VOXELGRAPHCORE_API FVoxelSpawnChunkExecNodeRuntime : public TVoxelExecNodeRuntime<FVoxelSpawnChunkExecNode>
{
public:
	using Super::Super;

	//~ Begin FVoxelExecNodeRuntime Interface
	virtual void Create() override;
	virtual void Destroy() override;
	virtual FVoxelOptionalBox GetBounds() const override;
	virtual TValue<FVoxelSpawnable> GenerateSpawnable(const FVoxelMergeSpawnableRef& Ref) override;
	//~ End FVoxelExecNodeRuntime Interface

private:
	TVoxelDynamicValue<FVoxelSpawnable> SpawnableValue;

	mutable FVoxelFastCriticalSection CriticalSection;
	FVoxelOptionalBox Bounds_RequiresLock;
	TSharedPtr<FVoxelSpawnable> Spawnable_RequiresLock;
	TSharedPtr<TVoxelFutureValueState<FVoxelSpawnable>> FutureSpawnable_RequiresLock;
};