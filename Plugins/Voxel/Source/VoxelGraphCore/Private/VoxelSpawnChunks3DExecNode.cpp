// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSpawnChunks3DExecNode.h"
#include "VoxelRuntime.h"
#include "VoxelInvoker.h"
#include "VoxelChunkSpawner.h"
#include "VoxelSpawnableRef.h"

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelSpawnChunks3DExecNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	return MakeVoxelUnique<FVoxelSpawnChunks3DExecNodeRuntime>(SharedThis);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnChunks3DExecNodeRuntime::Create()
{
	VOXEL_FUNCTION_COUNTER();

	PinValuesProvider.Compute(this, [this](const FPinValues& PinValues)
	{
		FVoxelUtilities::RunOnGameThread(MakeWeakPtrLambda(this, [=]
		{
			const int32 ChunkSize = FMath::CeilToInt(FMath::Clamp(PinValues.ChunkSize, 100, 100000000));

			if (!Octree_GameThread ||
				Octree_GameThread->ChunkSize != ChunkSize)
			{
				Octree_GameThread = MakeVoxelShared<FOctree>(ChunkSize);
				bUpdateQueued = true;
			}

			if (MinSpawnDistance_GameThread != PinValues.MinSpawnDistance ||
				MaxSpawnDistance_GameThread != PinValues.MaxSpawnDistance ||
				InvokerChannel_GameThread != PinValues.InvokerChannel)
			{
				MinSpawnDistance_GameThread = PinValues.MinSpawnDistance;
				MaxSpawnDistance_GameThread = PinValues.MaxSpawnDistance;
				InvokerChannel_GameThread = PinValues.InvokerChannel;

				bUpdateQueued = true;
			}
		}));
	});
}

void FVoxelSpawnChunks3DExecNodeRuntime::Destroy()
{
	VOXEL_FUNCTION_COUNTER();

	Octree_GameThread.Reset();
}

void FVoxelSpawnChunks3DExecNodeRuntime::Tick(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();

	if (bTaskInProgress ||
		!Octree_GameThread)
	{
		return;
	}

	const FMatrix LocalToWorld = Runtime.GetLocalToWorld().Get_NoDependency();

	TOptional<FVector> ViewOrigin = FVector::ZeroVector;
	if (!FVoxelGameUtilities::GetCameraView(Runtime.GetWorld_GameThread(), ViewOrigin.GetValue()))
	{
		ViewOrigin.Reset();
	}

	const TSharedRef<const FVoxelInvoker> Invoker = GVoxelInvokerManager->GetInvoker(Runtime.GetWorld(), InvokerChannel_GameThread);

	if (!LocalToWorld.Equals(LastLocalToWorld) ||
		ViewOrigin.IsSet() != LastViewOrigin.IsSet() ||
		(ViewOrigin.IsSet() && FVector::Distance(ViewOrigin.GetValue(), LastViewOrigin.GetValue()) >= GVoxelChunkSpawnerCameraRefreshThreshold) ||
		Invoker != LastInvoker)
	{
		LastLocalToWorld = LocalToWorld;
		LastViewOrigin = ViewOrigin;
		LastInvoker = Invoker;
		bUpdateQueued = true;
	}

	if (bUpdateQueued)
	{
		UpdateTree();
	}
}

TVoxelFutureValue<FVoxelSpawnable> FVoxelSpawnChunks3DExecNodeRuntime::GenerateSpawnable(const FVoxelMergeSpawnableRef& Ref)
{
	VOXEL_FUNCTION_COUNTER();

	if (Ref.NodePath != GetContext()->Path ||
		!ensure(Ref.NodeData.IsA<FVoxelSpawnChunks3DSpawnData>()))
	{
		return {};
	}

	const FVoxelSpawnChunks3DSpawnData SpawnInfo = Ref.NodeData.Get<FVoxelSpawnChunks3DSpawnData>();

	VOXEL_SCOPE_LOCK(Octree_GameThread->CriticalSection);

	TSharedPtr<FChunk>& Chunk = Octree_GameThread->Chunks_RequiresLock.FindOrAdd(SpawnInfo.ChunkKey);
	if (!Chunk)
	{
		const FVoxelIntBox Bounds = FVoxelIntBox(SpawnInfo.ChunkKey).Scale(Octree_GameThread->ChunkSize);
		Chunk = MakeVoxelShared<FChunk>(SpawnInfo.ChunkKey, Bounds);
		CreateChunk(*Chunk);
	}

	VOXEL_SCOPE_LOCK(Chunk->CriticalSection);

	if (Chunk->Spawnable_RequiresLock)
	{
		return Chunk->Spawnable_RequiresLock;
	}

	if (!Chunk->FutureValueState_RequiresLock)
	{
		Chunk->FutureValueState_RequiresLock = MakeVoxelShared<TVoxelFutureValueState<FVoxelSpawnable>>();
	}
	return Chunk->FutureValueState_RequiresLock.ToSharedRef();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnChunks3DExecNodeRuntime::UpdateTree()
{
	VOXEL_FUNCTION_COUNTER();

	ensure(bUpdateQueued);
	bUpdateQueued = false;

	ensure(!bTaskInProgress);
	bTaskInProgress = true;

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, MakeWeakPtrLambda(this, [
		this,
		LocalToWorld = LastLocalToWorld,
		ViewOrigin = LastViewOrigin,
		Invoker = LastInvoker.ToSharedRef(),
		MinSpawnDistance = MinSpawnDistance_GameThread,
		MaxSpawnDistance = MaxSpawnDistance_GameThread,
		Octree = Octree_GameThread]
	{
		VOXEL_SCOPE_COUNTER_FORMAT("Update octree NumNodes=%d", Octree->NumNodes());
		VOXEL_SCOPE_LOCK(Octree->CriticalSection);

		TVoxelArray<TSharedPtr<FChunk>> QueuedChunks;
		Octree->Update(
			[&](const FOctree::FNodeRef TreeNodeRef)
			{
				if (Octree->NumNodes() > GVoxelChunkSpawnerMaxChunks)
				{
					VOXEL_MESSAGE(Error, "{0}: More than {1} point chunks, aborting", this, GVoxelChunkSpawnerMaxChunks);
					return false;
				}

				const FVoxelBox NodeBounds = TreeNodeRef.GetBounds().ToVoxelBox().Scale(Octree->ChunkSize);

				if (ViewOrigin.IsSet())
				{
					const double Distance = NodeBounds.DistanceFromBoxToPoint(LocalToWorld.InverseTransformPosition(ViewOrigin.GetValue()));

					if (MinSpawnDistance - NodeBounds.Size().Size() <= Distance &&
						Distance <= MaxSpawnDistance)
					{
						return true;
					}
				}

				return Invoker->Intersect(NodeBounds.TransformBy(LocalToWorld));
			},
			[&](const FOctree::FNodeRef TreeNodeRef)
			{
				if (TreeNodeRef.GetHeight() > 0)
				{
					return;
				}

				const FVoxelIntBox Bounds = FVoxelIntBox(TreeNodeRef.GetMin()).Scale(Octree->ChunkSize);

				TSharedPtr<FChunk>& Chunk = Octree->Chunks_RequiresLock.FindOrAdd(TreeNodeRef.GetMin());
				ensure(!Chunk);
				Chunk = MakeVoxelShared<FChunk>(TreeNodeRef.GetMin(), Bounds);

				QueuedChunks.Add(Chunk);
			},
			[&](const FOctree::FNodeRef TreeNodeRef)
			{
				if (TreeNodeRef.GetHeight() > 0)
				{
					return;
				}

				ensure(Octree->Chunks_RequiresLock.Remove(TreeNodeRef.GetMin()));
			});

		if (Octree->NumNodes() >= GVoxelChunkSpawnerMaxChunks)
		{
			return;
		}

		for (const TSharedPtr<FChunk>& Chunk : QueuedChunks)
		{
			ensure(Chunk->Bounds.Size() == Octree->ChunkSize);
			CreateChunk(*Chunk);
		}

		AsyncTask(ENamedThreads::GameThread, MakeWeakPtrLambda(this, [this]
		{
			check(IsInGameThread());

			ensure(bTaskInProgress);
			bTaskInProgress = false;
		}));
	}));
}

void FVoxelSpawnChunks3DExecNodeRuntime::CreateChunk(FChunk& Chunk) const
{
	VOXEL_FUNCTION_COUNTER();

	ensure(!Chunk.SpawnableValue.IsValid());
	ensure(!Chunk.Spawnable_RequiresLock.IsValid());

	FVoxelSpawnChunks3DSpawnData SpawnData;
	SpawnData.ChunkKey = Chunk.Key;

	FVoxelMergeSpawnableRef MergeSpawnableRef(*GetContext());
	MergeSpawnableRef.NodeData = FVoxelInstancedStruct::Make(SpawnData);

	const TSharedRef<FVoxelQueryParameters> Parameters = MakeVoxelShared<FVoxelQueryParameters>();
	Parameters->Add<FVoxelSpawnableBoundsQueryParameter>().Bounds = Chunk.Bounds.ToVoxelBox();
	Parameters->Add<FVoxelMergeSpawnableRefQueryParameter>().MergeSpawnableRef = MergeSpawnableRef;

	Chunk.SpawnableValue =
		GetNodeRuntime().MakeDynamicValueFactory(Node.SpawnablePin)
		.AddRef(NodeRef)
		.Compute(GetContext(), Parameters);

	Chunk.SpawnableValue.OnChanged(MakeWeakPtrLambda(Chunk, [&Chunk](const TSharedRef<const FVoxelSpawnable>& NewSpawnable)
	{
		VOXEL_SCOPE_LOCK(Chunk.CriticalSection);

		if (Chunk.Spawnable_RequiresLock)
		{
			Chunk.Spawnable_RequiresLock->CallDestroy_AnyThread();
			Chunk.Spawnable_RequiresLock.Reset();
		}

		if (!NewSpawnable->IsValid())
		{
			return;
		}

		Chunk.Spawnable_RequiresLock = ConstCast(NewSpawnable);
		Chunk.Spawnable_RequiresLock->CallCreate_AnyThread();

		if (Chunk.FutureValueState_RequiresLock)
		{
			Chunk.FutureValueState_RequiresLock->SetValue(NewSpawnable);
			Chunk.FutureValueState_RequiresLock.Reset();
		}
	}));
}