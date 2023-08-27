// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#if 0 // TODO
#include "VoxelMinimal.h"
#include "VoxelExecNode.h"
#include "VoxelFastOctree.h"
#include "Point/VoxelPointSet.h"
#include "VoxelGenerateGrid3DExecNode.generated.h"

// Generate points in a 3D grid pattern
USTRUCT(DisplayName = "Generate Grid 3D")
struct VOXELGRAPHNODES_API FVoxelGenerateGrid3DExecNode : public FVoxelExecNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(float, CellSize, 100.f, VirtualPin);
	VOXEL_INPUT_PIN(int32, ChunkSize, 32, VirtualPin);
	VOXEL_INPUT_PIN(float, MinSpawnDistance, 0.f, VirtualPin);
	VOXEL_INPUT_PIN(float, MaxSpawnDistance, 1000.f, VirtualPin);
	VOXEL_OUTPUT_PIN(FVoxelPointSet, Out, VirtualPin);

	virtual TVoxelUniquePtr<FVoxelExecNodeRuntime> CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const override;
};

class VOXELGRAPHNODES_API FVoxelGenerateGrid3DExecNodeRuntime : public TVoxelExecNodeRuntime<FVoxelGenerateGrid3DExecNode>
{
public:
	using Super::Super;

	//~ Begin FVoxelExecNodeRuntime Interface
	virtual void Create() override;
	virtual void Tick(FVoxelRuntime& Runtime) override;
	//~ End FVoxelExecNodeRuntime Interface

private:
	struct FChunk
	{
		const FVoxelIntBox Bounds;
		TSharedPtr<FVoxelValueExecutor> Executor;

		explicit FChunk(const FVoxelIntBox& Bounds)
			: Bounds(Bounds)
		{
		}
	};
	struct FOctree : TVoxelFastOctree<>
	{
		const float CellSize;
		const int32 ChunkSize;

		TSharedPtr<FOctree> Tree;
		TVoxelIntVectorMap<TSharedPtr<FChunk>> Chunks;

		FOctree(
			const float CellSize,
			const int32 ChunkSize)
			: TVoxelFastOctree<>(30)
			, CellSize(CellSize)
			, ChunkSize(ChunkSize)
		{
		}
	};

private:
	DECLARE_VOXEL_PIN_VALUES(
		CellSize,
		ChunkSize,
		MinSpawnDistance,
		MaxSpawnDistance);

	bool bTaskInProgress = false;
	bool bUpdateQueued = false;
	float MinSpawnDistance = 0.f;
	float MaxSpawnDistance = 0.f;
	TSharedPtr<FOctree> Octree;
	TOptional<FVector> LastViewOrigin;

	void UpdateTree(const FVector& ViewOrigin);
};
#endif