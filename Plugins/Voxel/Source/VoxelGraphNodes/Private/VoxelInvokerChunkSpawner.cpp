// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelInvokerChunkSpawner.h"
#include "VoxelInvoker.h"
#include "VoxelRuntime.h"

BEGIN_VOXEL_NAMESPACE(InvokerChunkSpawner)

DEFINE_UNIQUE_VOXEL_ID(FChunkId);

END_VOXEL_NAMESPACE(InvokerChunkSpawner)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInvokerChunkSpawnerImpl::Tick(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();

	if (bTaskInProgress)
	{
		return;
	}

	ON_SCOPE_EXIT
	{
		if (bUpdateQueued)
		{
			UpdateTree();
		}
	};

	const FMatrix LocalToWorld = Runtime.GetLocalToWorld().Get_NoDependency();
	const TSharedRef<const FVoxelInvoker> Invoker = GVoxelInvokerManager->GetInvoker(Runtime.GetWorld(), InvokerChannel);

	if (!LocalToWorld.Equals(LastLocalToWorld) ||
		Invoker != LastInvoker)
	{
		LastLocalToWorld = LocalToWorld;
		LastInvoker = Invoker;
		bUpdateQueued = true;
	}
}

void FVoxelInvokerChunkSpawnerImpl::Refresh()
{
	bUpdateQueued = true;
}

void FVoxelInvokerChunkSpawnerImpl::Recreate()
{
	bUpdateQueued = true;
	bRecreateQueued = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInvokerChunkSpawnerImpl::UpdateTree()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_USE_NAMESPACE(InvokerChunkSpawner);

	const int64 SizeInChunks = FMath::Max<int64>(FMath::CeilToInt64(WorldSize / (VoxelSize * (ChunkSize << LOD))), 2);
	const int32 OctreeDepth = FMath::Min<int32>(FMath::CeilLogTwo64(SizeInChunks), 29);

	ensure(bUpdateQueued);
	bUpdateQueued = false;

	ensure(!bTaskInProgress);
	bTaskInProgress = true;

	if (bRecreateQueued)
	{
		Octree.Reset();
		bRecreateQueued = false;

		VOXEL_SCOPE_LOCK(CriticalSection);
		ChunkRefs_RequiresLock.Empty();
	}

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, MakeWeakPtrLambda(this, [
		this,
		OctreeDepth,
		OldTree = Octree,
		LocalToWorld = LastLocalToWorld,
		Invoker = LastInvoker.ToSharedRef()]
	{
		const TSharedRef<FOctree> NewTree = MakeVoxelShared<FOctree>(OctreeDepth, *this);

		if (OldTree)
		{
			VOXEL_SCOPE_COUNTER("CopyFrom");
			NewTree->CopyFrom(*OldTree);
		}

		TMap<FChunkId, FChunkInfo> ChunkInfos;
		TSet<FChunkId> ChunksToAdd;
		TSet<FChunkId> ChunksToRemove;
		NewTree->Update(
			LocalToWorld,
			*Invoker,
			ChunkInfos,
			ChunksToAdd,
			ChunksToRemove);

		VOXEL_SCOPE_LOCK(CriticalSection);

		for (const FChunkId ChunkId : ChunksToRemove)
		{
			ensure(ChunkRefs_RequiresLock.Remove(ChunkId));
		}

		for (const FChunkId ChunkId : ChunksToAdd)
		{
			const FChunkInfo ChunkInfo = ChunkInfos[ChunkId];

			const TSharedRef<FVoxelChunkRef> ChunkRef = CreateChunk(
				LOD,
				ChunkSize,
				ChunkInfo.ChunkBounds);
			ChunkRef->Compute();

			ensure(!ChunkRefs_RequiresLock.Contains(ChunkId));
			ChunkRefs_RequiresLock.Add(ChunkId) = ChunkRef;
		}

		AsyncTask(ENamedThreads::GameThread, MakeWeakPtrLambda(this, [this, NewTree]
		{
			VOXEL_FUNCTION_COUNTER();
			check(IsInGameThread());

			ensure(bTaskInProgress);
			bTaskInProgress = false;

			if (NewTree->NumNodes() >= GVoxelChunkSpawnerMaxChunks)
			{
				VOXEL_MESSAGE(Error, "{0}: voxel.chunkspawner.MaxChunks reached", NodeRef);
			}

			Octree = NewTree;
		}));
	}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BEGIN_VOXEL_NAMESPACE(InvokerChunkSpawner)

void FOctree::Update(
	const FMatrix& LocalToWorld,
	const FVoxelInvoker& Invoker,
	TMap<FChunkId, FChunkInfo>& ChunkInfos,
	TSet<FChunkId>& ChunksToAdd,
	TSet<FChunkId>& ChunksToRemove)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelFastOctree::Update(
		[&](const FNodeRef NodeRef)
		{
			if (NumNodes() > GVoxelChunkSpawnerMaxChunks)
			{
				return false;
			}

			return Invoker.Intersect(GetChunkBounds(NodeRef).TransformBy(LocalToWorld));
		},
		[&](const FNodeRef NodeRef)
		{
			if (NodeRef.GetHeight() > 0)
			{
				return;
			}

			FChunkId& ChunkId = GetNode(NodeRef);

			ensure(!ChunkId.IsValid());
			ChunkId = FChunkId::New();

			ChunkInfos.Add(ChunkId,
			{
				GetChunkBounds(NodeRef),
				NodeRef.GetBounds()
			});
			ChunksToAdd.Add(ChunkId);
		},
		[&](const FNodeRef NodeRef)
		{
			if (NodeRef.GetHeight() > 0)
			{
				return;
			}

			const FChunkId ChunkId = GetNode(NodeRef);
			ensure(ChunkId.IsValid());

			ChunkInfos.Add(ChunkId,
			{
				GetChunkBounds(NodeRef),
				NodeRef.GetBounds()
			});
			ChunksToRemove.Add(ChunkId);
		});
}

END_VOXEL_NAMESPACE(InvokerChunkSpawner)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_MakeInvokerChunkSpawner, Spawner)
{
	const TValue<int32> LOD = Get(LODPin, Query);
	const TValue<float> WorldSize = Get(WorldSizePin, Query);
	const TValue<int32> ChunkSize = Get(ChunkSizePin, Query);
	const TValue<FName> InvokerChannel = Get(InvokerChannelPin, Query);

	return VOXEL_ON_COMPLETE(LOD, WorldSize, ChunkSize, InvokerChannel)
	{
		const TSharedRef<FVoxelInvokerChunkSpawner> Spawner = MakeVoxelShared<FVoxelInvokerChunkSpawner>();
		Spawner->Node = GetNodeRef();
		Spawner->LOD = FMath::Clamp(LOD, 0, 30);
		Spawner->WorldSize = WorldSize;
		Spawner->ChunkSize = FMath::Clamp(FMath::CeilToInt(ChunkSize / 2.f) * 2, 4, 128);
		Spawner->InvokerChannel = InvokerChannel;
		return Spawner;
	};
}