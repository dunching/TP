// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelExecNode.h"
#include "VoxelSpawnable.h"
#include "VoxelFastOctree.h"
#include "VoxelSpawnChunks3DExecNode.generated.h"

struct FVoxelInvoker;

USTRUCT(DisplayName = "Spawn Chunks 3D")
struct VOXELGRAPHCORE_API FVoxelSpawnChunks3DExecNode : public FVoxelExecNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelSpawnable, Spawnable, nullptr, VirtualPin);
	// Chunk size in this actor local space
	VOXEL_INPUT_PIN(float, ChunkSize, 50000, VirtualPin);
	VOXEL_INPUT_PIN(float, MinSpawnDistance, 0.f, VirtualPin);
	VOXEL_INPUT_PIN(float, MaxSpawnDistance, 10000.f, VirtualPin);
	// Invoker channel to use in addition to camera position
	VOXEL_INPUT_PIN(FName, InvokerChannel, nullptr, VirtualPin);

	virtual TVoxelUniquePtr<FVoxelExecNodeRuntime> CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const override;
};

USTRUCT()
struct FVoxelSpawnChunks3DSpawnData : public FVoxelVirtualStruct
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	UPROPERTY()
	FIntVector ChunkKey = FIntVector::ZeroValue;
};

class VOXELGRAPHCORE_API FVoxelSpawnChunks3DExecNodeRuntime : public TVoxelExecNodeRuntime<FVoxelSpawnChunks3DExecNode>
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
		const FIntVector Key;
		const FVoxelIntBox Bounds;
		TVoxelDynamicValue<FVoxelSpawnable> SpawnableValue;

		FVoxelFastCriticalSection CriticalSection;
		TSharedPtr<FVoxelSpawnable> Spawnable_RequiresLock;
		TSharedPtr<TVoxelFutureValueState<FVoxelSpawnable>> FutureValueState_RequiresLock;

		FChunk(
			const FIntVector& Key,
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
		TVoxelIntVectorMap<TSharedPtr<FChunk>> Chunks_RequiresLock;

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
		MaxSpawnDistance,
		InvokerChannel);

	bool bTaskInProgress = false;
	bool bUpdateQueued = false;
	FMatrix LastLocalToWorld = FMatrix::Identity;
	TOptional<FVector> LastViewOrigin = FVector::ZeroVector;
	TSharedPtr<const FVoxelInvoker> LastInvoker;

	float MinSpawnDistance_GameThread = 0.f;
	float MaxSpawnDistance_GameThread = 0.f;
	FName InvokerChannel_GameThread;
	TSharedPtr<FOctree> Octree_GameThread;

	void UpdateTree();
	void CreateChunk(FChunk& Chunk) const;
};