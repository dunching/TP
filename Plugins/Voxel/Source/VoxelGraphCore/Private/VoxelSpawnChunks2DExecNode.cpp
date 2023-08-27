// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSpawnChunks2DExecNode.h"
#include "VoxelRuntime.h"
#include "VoxelChunkSpawner.h"

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelSpawnChunks2DExecNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	return MakeVoxelUnique<FVoxelSpawnChunks2DExecNodeRuntime>(SharedThis);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnChunks2DExecNodeRuntime::Create()
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
				MaxSpawnDistance_GameThread != PinValues.MaxSpawnDistance)
			{
				MinSpawnDistance_GameThread = PinValues.MinSpawnDistance;
				MaxSpawnDistance_GameThread = PinValues.MaxSpawnDistance;

				bUpdateQueued = true;
			}
		}));
	});
}

void FVoxelSpawnChunks2DExecNodeRuntime::Destroy()
{
	VOXEL_FUNCTION_COUNTER();

	Octree_GameThread.Reset();
}

void FVoxelSpawnChunks2DExecNodeRuntime::Tick(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();

	if (bTaskInProgress ||
		!Octree_GameThread)
	{
		return;
	}

	ON_SCOPE_EXIT
	{
		if (bUpdateQueued && LastViewOrigin.IsSet())
		{
			UpdateTree(LastViewOrigin.GetValue());
		}
	};

	FVector ViewOrigin = FVector::ZeroVector;
	if (!FVoxelGameUtilities::GetCameraView(Runtime.GetWorld_GameThread(), ViewOrigin))
	{
		return;
	}

	ViewOrigin = Runtime.GetLocalToWorld().Get_NoDependency().InverseTransformPosition(ViewOrigin);

	if (LastViewOrigin.IsSet() &&
		FVector::Distance(ViewOrigin, LastViewOrigin.GetValue()) < GVoxelChunkSpawnerCameraRefreshThreshold)
	{
		return;
	}

	LastViewOrigin = ViewOrigin;
	bUpdateQueued = true;
}

TVoxelFutureValue<FVoxelSpawnable> FVoxelSpawnChunks2DExecNodeRuntime::GenerateSpawnable(const FVoxelMergeSpawnableRef& Ref)
{
	VOXEL_FUNCTION_COUNTER();

	if (Ref.NodePath != GetContext()->Path ||
		!ensure(Ref.NodeData.IsA<FVoxelSpawnChunks2DSpawnData>()))
	{
		return {};
	}

	const FVoxelSpawnChunks2DSpawnData SpawnInfo = Ref.NodeData.Get<FVoxelSpawnChunks2DSpawnData>();

	VOXEL_SCOPE_LOCK(Octree_GameThread->CriticalSection);

	TSharedPtr<FChunk>& Chunk = Octree_GameThread->Chunks_RequiresLock.FindOrAdd(SpawnInfo.ChunkKey);
	if (!Chunk)
	{
		const FVoxelIntBox Bounds = FVoxelIntBox(FIntVector(
			SpawnInfo.ChunkKey.X,
			SpawnInfo.ChunkKey.Y,
			0)).Scale(Octree_GameThread->ChunkSize);
		ensure(Bounds.Min.Z == 0);

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

void FVoxelSpawnChunks2DExecNodeRuntime::UpdateTree(const FVector& ViewOrigin)
{
	VOXEL_FUNCTION_COUNTER();

	ensure(bUpdateQueued);
	bUpdateQueued = false;

	ensure(!bTaskInProgress);
	bTaskInProgress = true;

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, MakeWeakPtrLambda(this, [
		this,
		ViewOrigin,
		MinSpawnDistance = MinSpawnDistance_GameThread,
		MaxSpawnDistance = MaxSpawnDistance_GameThread,
		Octree = Octree_GameThread]
	{
		VOXEL_SCOPE_COUNTER("Update octree");
		VOXEL_SCOPE_LOCK(Octree->CriticalSection);

		TVoxelArray<TSharedPtr<FChunk>> QueuedChunks;
		Octree->Update(
			[&](const FOctree::FNodeRef TreeNodeRef)
			{
				if (TreeNodeRef.GetBounds().Min.Z != 0)
				{
					return false;
				}

				const FVoxelBox NodeBounds = TreeNodeRef.GetBounds().ToVoxelBox().Scale(Octree->ChunkSize);
				const double Distance = NodeBounds.DistanceFromBoxToPoint(ViewOrigin);

				if (Octree->NumNodes() > GVoxelChunkSpawnerMaxChunks)
				{
					VOXEL_MESSAGE(Error, "{0}: More than {1} point chunks, aborting", this, GVoxelChunkSpawnerMaxChunks);
					return false;
				}

				return
					MinSpawnDistance - NodeBounds.Size().Size() <= Distance &&
					Distance <= MaxSpawnDistance;
			},
			[&](const FOctree::FNodeRef TreeNodeRef)
			{
				if (TreeNodeRef.GetHeight() > 0)
				{
					return;
				}

				const FVoxelIntBox Bounds = TreeNodeRef.GetBounds().Scale(Octree->ChunkSize);
				ensure(Bounds.Min.Z == 0);

				const FIntPoint Key(
					TreeNodeRef.GetMin().X,
					TreeNodeRef.GetMin().Y);
				ensure(TreeNodeRef.GetMin().Z == 0);

				TSharedPtr<FChunk>& Chunk = Octree->Chunks_RequiresLock.FindOrAdd(Key);
				ensure(!Chunk);
				Chunk = MakeVoxelShared<FChunk>(Key, Bounds);

				QueuedChunks.Add(Chunk);
			},
			[&](const FOctree::FNodeRef TreeNodeRef)
			{
				if (TreeNodeRef.GetHeight() > 0)
				{
					return;
				}

				const FIntPoint Key(
					TreeNodeRef.GetMin().X,
					TreeNodeRef.GetMin().Y);
				ensure(TreeNodeRef.GetMin().Z == 0);

				ensure(Octree->Chunks_RequiresLock.Remove(Key));
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

void FVoxelSpawnChunks2DExecNodeRuntime::CreateChunk(FChunk& Chunk) const
{
	VOXEL_FUNCTION_COUNTER();

	ensure(!Chunk.SpawnableValue.IsValid());
	ensure(!Chunk.Spawnable_RequiresLock.IsValid());

	FVoxelBox Bounds = Chunk.Bounds.ToVoxelBox();
	ensure(Bounds.Min.Z == 0);
	Bounds.Min.Z = FVoxelBox::Infinite.Min.Z;
	Bounds.Max.Z = FVoxelBox::Infinite.Max.Z;

	FVoxelSpawnChunks2DSpawnData SpawnData;
	SpawnData.ChunkKey = Chunk.Key;

	FVoxelMergeSpawnableRef MergeSpawnableRef(*GetContext());
	MergeSpawnableRef.NodeData = FVoxelInstancedStruct::Make(SpawnData);

	const TSharedRef<FVoxelQueryParameters> Parameters = MakeVoxelShared<FVoxelQueryParameters>();
	Parameters->Add<FVoxelSpawnableBoundsQueryParameter>().Bounds = Bounds;
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

		if (NewSpawnable->IsValid())
		{
			Chunk.Spawnable_RequiresLock = ConstCast(NewSpawnable);
			Chunk.Spawnable_RequiresLock->CallCreate_AnyThread();
		}

		if (Chunk.FutureValueState_RequiresLock)
		{
			Chunk.FutureValueState_RequiresLock->SetValue(NewSpawnable);
			Chunk.FutureValueState_RequiresLock.Reset();
		}
	}));
}