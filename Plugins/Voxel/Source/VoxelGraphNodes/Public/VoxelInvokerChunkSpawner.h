// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelFastOctree.h"
#include "VoxelChunkSpawner.h"
#include "VoxelInvokerChunkSpawner.generated.h"

struct FVoxelInvoker;

BEGIN_VOXEL_NAMESPACE(InvokerChunkSpawner)

class FOctree;
DECLARE_UNIQUE_VOXEL_ID(FChunkId);

END_VOXEL_NAMESPACE(InvokerChunkSpawner)

struct VOXELGRAPHNODES_API FVoxelInvokerChunkSpawnerImpl : public FVoxelChunkSpawnerImpl
{
public:
	const FVoxelGraphNodeRef NodeRef;
	const int32 LOD;
	const float WorldSize;
	const int32 ChunkSize;
	const FName InvokerChannel;

	FVoxelInvokerChunkSpawnerImpl(
		const FVoxelGraphNodeRef& NodeRef,
		const int32 LOD,
		const float WorldSize,
		const int32 ChunkSize,
		const FName InvokerChannel)
		: NodeRef(NodeRef)
		, LOD(LOD)
		, WorldSize(WorldSize)
		, ChunkSize(ChunkSize)
		, InvokerChannel(InvokerChannel)
	{
	}

	//~ Begin FVoxelChunkSpawnerImpl Interface
	virtual void Tick(FVoxelRuntime& Runtime) override;
	virtual void Refresh() override;
	virtual void Recreate() override;
	//~ End FVoxelChunkSpawnerImpl Interface

private:
	VOXEL_USE_NAMESPACE_TYPES(InvokerChunkSpawner, FOctree, FChunkId);

	TSharedPtr<const FOctree> Octree;
	bool bTaskInProgress = false;
	bool bUpdateQueued = false;
	bool bRecreateQueued = false;
	FMatrix LastLocalToWorld = FMatrix::Identity;
	TSharedPtr<const FVoxelInvoker> LastInvoker;

	FVoxelCriticalSection CriticalSection;
	TMap<FChunkId, TSharedPtr<FVoxelChunkRef>> ChunkRefs_RequiresLock;

	void UpdateTree();
};

BEGIN_VOXEL_NAMESPACE(InvokerChunkSpawner)

struct FChunkInfo
{
	FVoxelBox ChunkBounds;
	FVoxelIntBox NodeBounds;
};

class FOctree : public TVoxelFastOctree<FChunkId>
{
public:
	const FVoxelInvokerChunkSpawnerImpl& Object;

	FOctree(
		const int32 Depth,
		const FVoxelInvokerChunkSpawnerImpl& Object)
		: TVoxelFastOctree(Depth)
		, Object(Object)
	{
	}

	FORCEINLINE FVoxelBox GetChunkBounds(const FNodeRef NodeRef) const
	{
		return NodeRef.GetBounds().ToVoxelBox().Scale(Object.VoxelSize * (Object.ChunkSize << Object.LOD));
	}

	void Update(
		const FMatrix& LocalToWorld,
		const FVoxelInvoker& Invoker,
		TMap<FChunkId, FChunkInfo>& ChunkInfos,
		TSet<FChunkId>& ChunksToAdd,
		TSet<FChunkId>& ChunksToRemove);
};

END_VOXEL_NAMESPACE(InvokerChunkSpawner)

USTRUCT()
struct VOXELGRAPHNODES_API FVoxelInvokerChunkSpawner : public FVoxelChunkSpawner
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	FVoxelGraphNodeRef Node;
	int32 LOD = 0;
	float WorldSize = 0;
	int32 ChunkSize = 0;
	FName InvokerChannel;

	virtual TSharedPtr<FVoxelChunkSpawnerImpl> MakeImpl() const override
	{
		return MakeVoxelShared<FVoxelInvokerChunkSpawnerImpl>(Node, LOD, WorldSize, ChunkSize, InvokerChannel);
	}
};

USTRUCT(Category = "Chunk Spawner")
struct VOXELGRAPHNODES_API FVoxelNode_MakeInvokerChunkSpawner : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(int32, LOD, 0);
	VOXEL_INPUT_PIN(float, WorldSize, 1.e6f);
	VOXEL_INPUT_PIN(int32, ChunkSize, 32);
	VOXEL_INPUT_PIN(FName, InvokerChannel, "Default");

	VOXEL_OUTPUT_PIN(FVoxelChunkSpawner, Spawner);
};