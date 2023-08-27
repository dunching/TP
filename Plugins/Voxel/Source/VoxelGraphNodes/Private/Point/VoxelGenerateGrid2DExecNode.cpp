// Copyright Voxel Plugin, Inc. All Rights Reserved.

#if 0 // TODO
#include "Point/VoxelGenerateGrid2DExecNode.h"
#include "VoxelRuntime.h"
#include "VoxelChunkSpawner.h"

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelGenerateGrid2DExecNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	return MakeVoxelUnique<FVoxelGenerateGrid2DExecNodeRuntime>(SharedThis);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGenerateGrid2DExecNodeRuntime::Create()
{
	VOXEL_FUNCTION_COUNTER();

	PinValuesProvider.Compute(this, [this](const FPinValues& PinValues)
	{
		FVoxelUtilities::RunOnGameThread(MakeWeakPtrLambda(this, [=]
		{
			if (!Octree ||
				Octree->CellSize != PinValues.CellSize ||
				Octree->ChunkSize != PinValues.ChunkSize)
			{
				Octree = MakeVoxelShared<FOctree>(PinValues.CellSize, PinValues.ChunkSize);
				bUpdateQueued = true;
			}

			if (MinSpawnDistance != PinValues.MinSpawnDistance ||
				MaxSpawnDistance != PinValues.MaxSpawnDistance)
			{
				MinSpawnDistance = PinValues.MinSpawnDistance;
				MaxSpawnDistance = PinValues.MaxSpawnDistance;

				bUpdateQueued = true;
			}
		}));
	});
}

void FVoxelGenerateGrid2DExecNodeRuntime::Tick(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();

	if (bTaskInProgress ||
		!Octree)
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGenerateGrid2DExecNodeRuntime::UpdateTree(const FVector& ViewOrigin)
{
	VOXEL_FUNCTION_COUNTER();

	ensure(bUpdateQueued);
	bUpdateQueued = false;

	ensure(!bTaskInProgress);
	bTaskInProgress = true;

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, MakeWeakPtrLambda(this, [=]
	{
		VOXEL_SCOPE_COUNTER("Update octree");

		if (!ensure(FMath::Square(double(Octree->ChunkSize)) < MAX_int32))
		{
			return;
		}

		TVoxelArray<TSharedPtr<FChunk>> QueuedChunks;
		Octree->Update(
			[&](const FOctree::FNodeRef TreeNodeRef)
			{
				if (TreeNodeRef.GetBounds().Min.Z != 0)
				{
					return false;
				}

				const FVoxelBox NodeBounds = TreeNodeRef.GetBounds().ToVoxelBox().Scale(Octree->CellSize * Octree->ChunkSize);
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

				TSharedPtr<FChunk>& Chunk = Octree->Chunks.FindOrAdd(TreeNodeRef.GetMin());
				ensure(!Chunk);
				Chunk = MakeVoxelShared<FChunk>(Bounds);

				QueuedChunks.Add(Chunk);
			},
			[&](const FOctree::FNodeRef TreeNodeRef)
			{
				if (TreeNodeRef.GetHeight() > 0)
				{
					return;
				}

				ensure(Octree->Chunks.Remove(TreeNodeRef.GetMin()));
			});

		if (Octree->NumNodes() >= GVoxelChunkSpawnerMaxChunks)
		{
			return;
		}

		for (const TSharedPtr<FChunk>& Chunk : QueuedChunks)
		{
			ensure(Chunk->Bounds.Size() == Octree->ChunkSize);
			Chunk->Executor =
				GetNodeRuntime().MakeValueExecutorFactory(Node.OutPin)
				.AddRef(NodeRef)
				.Execute(
					GetContext(),
					MakeVoxelShared<FVoxelQueryParameters>(),
					[Bounds = Chunk->Bounds, CellSize = Octree->CellSize](const FVoxelQuery& Query)
					{
						const int32 Num = Bounds.Size().X * Bounds.Size().Y;

						FVoxelPointIdBufferStorage Id;
						FVoxelFloatBufferStorage PositionX;
						FVoxelFloatBufferStorage PositionY;
						Id.Allocate(Num);
						PositionX.Allocate(Num);
						PositionY.Allocate(Num);
						{
							VOXEL_SCOPE_COUNTER("Make positions");

							int32 Index = 0;
							for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y++)
							{
								for (int32 X = Bounds.Min.X; X < Bounds.Max.X; X++)
								{
									Id[Index] = FVoxelUtilities::MurmurHashMulti(X, Y);
									PositionX[Index] = X * CellSize;
									PositionY[Index] = Y * CellSize;
									Index++;
								}
							}
						}

						FVoxelVectorBuffer Positions;
						Positions.X = FVoxelFloatBuffer::Make(PositionX);
						Positions.Y = FVoxelFloatBuffer::Make(PositionY);
						Positions.Z = 0.f;

						const TSharedRef<FVoxelPointSet> PointSet = MakeVoxelShared<FVoxelPointSet>();
						PointSet->SetNum(Num);
						PointSet->Add(FVoxelPointAttributes::Id, FVoxelPointIdBuffer::Make(Id));
						PointSet->Add(FVoxelPointAttributes::Position, Positions);
						return PointSet;
					});
		}

		AsyncTask(ENamedThreads::GameThread, MakeWeakPtrLambda(this, [=]
		{
			check(IsInGameThread());

			ensure(bTaskInProgress);
			bTaskInProgress = false;
		}));
	}));
}
#endif