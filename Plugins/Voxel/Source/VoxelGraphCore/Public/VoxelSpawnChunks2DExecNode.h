// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelExecNode.h"
#include "VoxelSpawnable.h"
#include "VoxelFastOctree.h"
#include "VoxelSpawnChunks2DExecNode.generated.h"

USTRUCT(DisplayName = "Spawn Chunks 2D")
struct VOXELGRAPHCORE_API FVoxelSpawnChunks2DExecNode : public FVoxelExecNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelSpawnable, Spawnable, nullptr, VirtualPin);
	// Chunk size in this actor local space
	VOXEL_INPUT_PIN(float, ChunkSize, 50000, VirtualPin);
	VOXEL_INPUT_PIN(float, MinSpawnDistance, 0.f, VirtualPin);
	VOXEL_INPUT_PIN(float, MaxSpawnDistance, 10000.f, VirtualPin);

	virtual TVoxelUniquePtr<FVoxelExecNodeRuntime> CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const override;
};

USTRUCT()
struct FVoxelSpawnChunks2DSpawnData : public FVoxelVirtualStruct
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	UPROPERTY()
	FIntPoint ChunkKey = FIntPoint::ZeroValue;
};

class VOXELGRAPHCORE_API FVoxelSpawnChunks2DExecNodeRuntime : public TVoxelExecNodeRuntime<FVoxelSpawnChunks2DExecNode>
{
public:
	using Super::Super;

	//~ Begin FVoxelExecNodeRuntime Interface
	virtual void Create() override;
	virtual void Destroy() override;
	virtual void Tick(FVoxelRuntime& Runtime) override;
	virtual TValue<FVoxelSpawnable> GenerateSpawnable(const FVoxelMergeSpawnableRef& Ref) override;
	//~ End FVoxelExecNodeRuntime Interface

private:
	struct FChunk : TSharedFromThis<FChunk>
	{
		const FIntPoint Key;
		const FVoxelIntBox Bounds;
		TVoxelDynamicValue<FVoxelSpawnable> SpawnableValue;

		FVoxelFastCriticalSection CriticalSection;
		TSharedPtr<FVoxelSpawnable> Spawnable_RequiresLock;
		TSharedPtr<TVoxelFutureValueState<FVoxelSpawnable>> FutureValueState_RequiresLock;

		FChunk(
			const FIntPoint& Key,
			const FVoxelIntBox& Bounds)
			: Key(Key)
			, Bounds(Bounds)
		{
		}
		~FChunk()
		{
			if (Spawnable_RequiresLock)
			{
				Spawnable_RequiresLock->CallDestroy_AnyThread();
				Spawnable_RequiresLock.Reset();
			}
		}
	};
	struct FOctree : TVoxelFastOctree<>
	{
		const int32 ChunkSize;

		FVoxelFastCriticalSection CriticalSection;
		TVoxelIntPointMap<TSharedPtr<FChunk>> Chunks_RequiresLock;

		explicit FOctree(const int32 ChunkSize)
			: TVoxelFastOctree<>(30)
			, ChunkSize(ChunkSize)
		{
		}
	};

private:
	DECLARE_VOXEL_PIN_VALUES(
		ChunkSize,
		MinSpawnDistance,
		MaxSpawnDistance);

	bool bTaskInProgress = false;
	bool bUpdateQueued = false;
	TOptional<FVector> LastViewOrigin;

	float MinSpawnDistance_GameThread = 0.f;
	float MaxSpawnDistance_GameThread = 0.f;
	TSharedPtr<FOctree> Octree_GameThread;

	void UpdateTree(const FVector& ViewOrigin);
	void CreateChunk(FChunk& Chunk) const;
};