// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelSurface.h"
#include "VoxelExecNode.h"
#include "VoxelFastOctree.h"
#include "VoxelPhysicalMaterial.h"
#include "VoxelMarchingCubeCollisionNode.generated.h"

struct FVoxelInvoker;
struct FVoxelCollider;
class UVoxelCollisionComponent;
class UVoxelNavigationComponent;

USTRUCT(DisplayName = "Generate Marching Cube Collision & Navmesh")
struct VOXELGRAPHNODES_API FVoxelMarchingCubeCollisionExecNode : public FVoxelExecNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelSurface, Surface, nullptr, VirtualPin);
	VOXEL_INPUT_PIN(FBodyInstance, BodyInstance, nullptr, VirtualPin);
	VOXEL_INPUT_PIN(FName, InvokerChannel, "Default", VirtualPin);
	VOXEL_INPUT_PIN(float, VoxelSize, 100.f, VirtualPin);
	VOXEL_INPUT_PIN(bool, ComputeCollision, true, VirtualPin);
	VOXEL_INPUT_PIN(bool, ComputeNavmesh, false, VirtualPin);

	VOXEL_INPUT_PIN(FVoxelPhysicalMaterialBuffer, PhysicalMaterial, nullptr, VirtualPin, AdvancedDisplay);
	VOXEL_INPUT_PIN(float, DistanceChecksTolerance, 1.f, VirtualPin, AdvancedDisplay);
	VOXEL_INPUT_PIN(int32, ChunkSize, 32, VirtualPin, AdvancedDisplay);

	TValue<FVoxelCollider> CreateCollider(
		const FVoxelQuery& InQuery,
		float VoxelSize,
		int32 ChunkSize,
		const FVoxelBox& Bounds) const;
	virtual TVoxelUniquePtr<FVoxelExecNodeRuntime> CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const override;
};

class VOXELGRAPHNODES_API FVoxelMarchingCubeCollisionExecNodeRuntime : public TVoxelExecNodeRuntime<FVoxelMarchingCubeCollisionExecNode>
{
public:
	using Super::Super;

	//~ Begin FVoxelExecNodeRuntime Interface
	virtual void Create() override;
	virtual void Destroy() override;
	virtual void Tick(FVoxelRuntime& Runtime) override;
	//~ End FVoxelExecNodeRuntime Interface

private:
	struct FOctree : TVoxelFastOctree<>
	{
		const float VoxelSize;
		const int32 ChunkSize;

		explicit FOctree(
			const float VoxelSize,
			const int32 ChunkSize)
			: TVoxelFastOctree(30)
			, VoxelSize(VoxelSize)
			, ChunkSize(ChunkSize)
		{
		}
	};
	struct FChunk
	{
		FChunk() = default;
		~FChunk()
		{
			ensure(!Collider_RequiresLock.IsValid());
			ensure(!CollisionComponent_GameThread.IsValid());
			ensure(!NavigationComponent_GameThread.IsValid());
		}

		TVoxelDynamicValue<FVoxelCollider> Collider_RequiresLock;
		TWeakObjectPtr<UVoxelCollisionComponent> CollisionComponent_GameThread;
		TWeakObjectPtr<UVoxelNavigationComponent> NavigationComponent_GameThread;
	};

public:
	DECLARE_VOXEL_PIN_VALUES(
		BodyInstance,
		InvokerChannel,
		VoxelSize,
		ChunkSize,
		ComputeCollision,
		ComputeNavmesh);

	FName InvokerChannel_GameThread;
	TSharedPtr<const FVoxelInvoker> LastInvoker_GameThread;
	TSharedPtr<const FBodyInstance> BodyInstance_GameThread;
	bool bComputeCollision_GameThread = false;
	bool bComputeNavmesh_GameThread = false;

	FVoxelFastCriticalSection CriticalSection;
	TSharedPtr<FOctree> Octree_RequiresLock;
	TVoxelIntVectorMap<TSharedPtr<FChunk>> Chunks_RequiresLock;

	struct FQueuedCollider
	{
		TWeakPtr<FChunk> Chunk;
		TSharedPtr<const FVoxelCollider> Collider;
	};
	TQueue<FQueuedCollider, EQueueMode::Mpsc> QueuedColliders;
	TQueue<TSharedPtr<FChunk>, EQueueMode::Mpsc> ChunksToDestroy;

	void Update_AnyThread(const TSharedRef<const FVoxelInvoker>& Invoker);

	void ProcessChunksToDestroy();
	void ProcessQueuedColliders(FVoxelRuntime& Runtime);
};