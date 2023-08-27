// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelFastOctree.h"
#include "VoxelChunkSpawner.h"
#include "VoxelScreenSizeChunkSpawner.generated.h"

BEGIN_VOXEL_NAMESPACE(ScreenSizeChunkSpawner)

class FOctree;

DECLARE_UNIQUE_VOXEL_ID(FChunkId);

END_VOXEL_NAMESPACE(ScreenSizeChunkSpawner)

struct VOXELGRAPHNODES_API FVoxelScreenSizeChunkSpawnerImpl : public FVoxelChunkSpawnerImpl
{
public:
	const FVoxelGraphNodeRef NodeRef;
	const float WorldSize;
	const int32 ChunkSize;
	const int32 MaxLOD;
	const bool bEnableTransitions;
	const TVoxelDynamicValue<float> ChunkScreenSizeValue;

	TOptional<float> ChunkScreenSize;

	FVoxelScreenSizeChunkSpawnerImpl(
		const FVoxelGraphNodeRef& NodeRef,
		const float WorldSize,
		const int32 ChunkSize,
		const int32 MaxLOD,
		const bool bEnableTransitions,
		const TVoxelDynamicValue<float>& ChunkScreenSizeValue)
		: NodeRef(NodeRef)
		, WorldSize(WorldSize)
		, ChunkSize(ChunkSize)
		, MaxLOD(MaxLOD)
		, bEnableTransitions(bEnableTransitions)
		, ChunkScreenSizeValue(ChunkScreenSizeValue)
	{
	}

	//~ Begin FVoxelChunkSpawnerImpl Interface
	virtual void Tick(FVoxelRuntime& Runtime) override;
	virtual void Refresh() override;
	virtual void Recreate() override;
	//~ End FVoxelChunkSpawnerImpl Interface

private:
	VOXEL_USE_NAMESPACE_TYPES(ScreenSizeChunkSpawner, FOctree, FChunkId);

	TSharedPtr<const FOctree> Octree;
	bool bTaskInProgress = false;
	bool bUpdateQueued = false;
	bool bRecreateQueued = false;
	TOptional<FVector> LastViewOrigin;

	struct FPreviousChunks
	{
		FPreviousChunks() = default;
		virtual ~FPreviousChunks() = default;

		TArray<TSharedPtr<FPreviousChunks>> Children;
	};
	struct FPreviousChunksLeaf : FPreviousChunks
	{
		TSharedPtr<FVoxelChunkRef> ChunkRef;
	};
	struct FChunk
	{
		TSharedPtr<FVoxelChunkRef> ChunkRef;
		TSharedPtr<FPreviousChunks> PreviousChunks;
	};

	FVoxelCriticalSection CriticalSection;
	TMap<FChunkId, TSharedPtr<FChunk>> Chunks_RequiresLock;

	void UpdateTree(const FVector& ViewOrigin);
};

BEGIN_VOXEL_NAMESPACE(ScreenSizeChunkSpawner)

struct FNode
{
	bool bIsRendered = false;
	uint8 TransitionMask = 0;
	FChunkId ChunkId = FChunkId::New();
};

struct FChunkInfo
{
	FVoxelBox ChunkBounds;
	int32 LOD = 0;
	uint8 TransitionMask = 0;
	FVoxelIntBox NodeBounds;
};

class FOctree : public TVoxelFastOctree<FNode>
{
public:
	const FVector ViewOrigin;
	const FVoxelScreenSizeChunkSpawnerImpl& Object;

	FOctree(
		const int32 Depth,
		const FVector& ViewOrigin,
		const FVoxelScreenSizeChunkSpawnerImpl& Object)
		: TVoxelFastOctree<FNode>(Depth)
		, ViewOrigin(ViewOrigin)
		, Object(Object)
	{
	}

	FORCEINLINE FVoxelBox GetChunkBounds(const FNodeRef NodeRef) const
	{
		return NodeRef.GetBounds().ToVoxelBox().Scale(Object.VoxelSize * Object.ChunkSize);
	}

	void Update(
		float ChunkScreenSize,
		TMap<FChunkId, FChunkInfo>& ChunkInfos,
		TSet<FChunkId>& ChunksToAdd,
		TSet<FChunkId>& ChunksToRemove,
		TSet<FChunkId>& ChunksToUpdate);

	bool AdjacentNodeHasHigherHeight(FNodeRef NodeRef, int32 Direction) const;
};

END_VOXEL_NAMESPACE(ScreenSizeChunkSpawner)

USTRUCT()
struct VOXELGRAPHNODES_API FVoxelScreenSizeChunkSpawner : public FVoxelChunkSpawner
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	FVoxelGraphNodeRef Node;
	float WorldSize = 1.e6f;
	int32 ChunkSize = 32;
	int32 MaxLOD = 20;
	bool bEnableTransitions = true;
	TVoxelDynamicValue<float> ChunkScreenSizeValue;

	virtual TSharedPtr<FVoxelChunkSpawnerImpl> MakeImpl() const override
	{
		const TSharedRef<FVoxelScreenSizeChunkSpawnerImpl> Impl = MakeVoxelShared<FVoxelScreenSizeChunkSpawnerImpl>(
			Node,
			WorldSize,
			ChunkSize,
			MaxLOD,
			bEnableTransitions,
			ChunkScreenSizeValue);

		if (Impl->ChunkScreenSizeValue.IsValid())
		{
			Impl->ChunkScreenSizeValue.OnChanged_GameThread(MakeWeakPtrLambda(Impl, [&Impl = *Impl](const float NewChunkScreenSize)
			{
				Impl.ChunkScreenSize = NewChunkScreenSize;
				Impl.Refresh();
			}));
		}
		else
		{
			Impl->ChunkScreenSize = 1.f;
			Impl->Refresh();
		}

		return Impl;
	}
};

USTRUCT(Category = "Chunk Spawner")
struct VOXELGRAPHNODES_API FVoxelNode_MakeScreenSizeChunkSpawner : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	// World size in cm
	VOXEL_INPUT_PIN(float, WorldSize, 1.e6f);
	// Size of a chunk in voxels
	VOXEL_INPUT_PIN(int32, ChunkSize, 32, AdvancedDisplay);
	// Relative size of a chunk on screen
	// Smaller = higher quality
	VOXEL_INPUT_PIN(float, ChunkScreenSize, 1.f, VirtualPin, AdvancedDisplay);
	// Max LOD to apply
	VOXEL_INPUT_PIN(int32, MaxLOD, 20, AdvancedDisplay);
	// Add transition meshes in-between LODs to hide holes
	VOXEL_INPUT_PIN(bool, EnableTransitions, true, AdvancedDisplay);

	VOXEL_OUTPUT_PIN(FVoxelChunkSpawner, Spawner);
};