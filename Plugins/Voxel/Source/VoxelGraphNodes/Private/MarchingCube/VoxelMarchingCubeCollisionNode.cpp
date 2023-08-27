// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "MarchingCube/VoxelMarchingCubeCollisionNode.h"
#include "MarchingCube/VoxelMarchingCubeNodes.h"
#include "VoxelInvoker.h"
#include "VoxelRuntime.h"
#include "Collision/VoxelCollisionComponent.h"
#include "Navigation/VoxelNavigationComponent.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELGRAPHNODES_API, bool, GVoxelCollisionEnableInEditor, false,
	"voxel.collision.EnableInEditor",
	"");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelNodeAliases::TValue<FVoxelCollider> FVoxelMarchingCubeCollisionExecNode::CreateCollider(
	const FVoxelQuery& InQuery,
	const float VoxelSize,
	const int32 ChunkSize,
	const FVoxelBox& Bounds) const
{
	checkVoxelSlow(FVoxelTaskReferencer::Get().IsReferenced(this));
	const FVoxelQuery Query = InQuery.EnterScope(*this);

	return VOXEL_CALL_NODE(FVoxelNode_CreateMarchingCubeCollider, ColliderPin, Query)
	{
		VOXEL_CALL_NODE_BIND(SurfacePin, VoxelSize, ChunkSize, Bounds)
		{
			return VOXEL_CALL_NODE(FVoxelNode_GenerateMarchingCubeSurface, SurfacePin, Query)
			{
				VOXEL_CALL_NODE_BIND(DistancePin)
				{
					const TValue<FVoxelSurface> Surface = GetNodeRuntime().Get(SurfacePin, Query);
					return VOXEL_ON_COMPLETE(Surface)
					{
						return Surface->GetDistance(Query);
					};
				};
				VOXEL_CALL_NODE_BIND(VoxelSizePin, VoxelSize)
				{
					return VoxelSize;
				};
				VOXEL_CALL_NODE_BIND(ChunkSizePin, ChunkSize)
				{
					return ChunkSize;
				};
				VOXEL_CALL_NODE_BIND(BoundsPin, Bounds)
				{
					return Bounds;
				};
				VOXEL_CALL_NODE_BIND(EnableTransitionsPin)
				{
					return false;
				};
				VOXEL_CALL_NODE_BIND(PerfectTransitionsPin)
				{
					return false;
				};
				VOXEL_CALL_NODE_BIND(EnableDistanceChecksPin)
				{
					return true;
				};
				VOXEL_CALL_NODE_BIND(DistanceChecksTolerancePin)
				{
					return GetNodeRuntime().Get(DistanceChecksTolerancePin, Query);
				};
			};
		};

		VOXEL_CALL_NODE_BIND(PhysicalMaterialPin)
		{
			return GetNodeRuntime().Get(PhysicalMaterialPin, Query);
		};
	};
}

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelMarchingCubeCollisionExecNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	return MakeVoxelUnique<FVoxelMarchingCubeCollisionExecNodeRuntime>(SharedThis);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMarchingCubeCollisionExecNodeRuntime::Create()
{
	VOXEL_FUNCTION_COUNTER();

	PinValuesProvider.Compute(this, [this](const FPinValues& PinValues)
	{
		FVoxelUtilities::RunOnGameThread(MakeWeakPtrLambda(this, [=]
		{
			InvokerChannel_GameThread = PinValues.InvokerChannel;
			BodyInstance_GameThread = PinValues.BodyInstance;
			bComputeCollision_GameThread = PinValues.ComputeCollision;
			bComputeNavmesh_GameThread = PinValues.ComputeNavmesh;

			VOXEL_SCOPE_LOCK(CriticalSection);

			if (!Octree_RequiresLock ||
				Octree_RequiresLock->VoxelSize != PinValues.VoxelSize ||
				Octree_RequiresLock->ChunkSize != PinValues.ChunkSize)
			{
				Octree_RequiresLock = MakeVoxelShared<FOctree>(PinValues.VoxelSize, PinValues.ChunkSize);

				const TSharedPtr<FVoxelRuntime> Runtime = GetRuntime();
				for (const auto& It : Chunks_RequiresLock)
				{
					FChunk& Chunk = *It.Value;
					Chunk.Collider_RequiresLock = {};

					if (Runtime)
					{
						Runtime->DestroyComponent(Chunk.CollisionComponent_GameThread);
						Runtime->DestroyComponent(Chunk.NavigationComponent_GameThread);
					}
					else
					{
						Chunk.CollisionComponent_GameThread.Reset();
						Chunk.NavigationComponent_GameThread.Reset();
					}
				}
				Chunks_RequiresLock.Empty();
			}

			if (BodyInstance_GameThread != PinValues.BodyInstance)
			{
				for (const auto& It : Chunks_RequiresLock)
				{
					if (UVoxelCollisionComponent* Component = It.Value->CollisionComponent_GameThread.Get())
					{
						Component->SetBodyInstance(*BodyInstance_GameThread);
					}
				}
			}
		}));
	});
}

void FVoxelMarchingCubeCollisionExecNodeRuntime::Destroy()
{
	VOXEL_FUNCTION_COUNTER();

	ProcessChunksToDestroy();
	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		const TSharedPtr<FVoxelRuntime> Runtime = GetRuntime();
		for (const auto& It : Chunks_RequiresLock)
		{
			FChunk& Chunk = *It.Value;
			Chunk.Collider_RequiresLock = {};

			if (Runtime)
			{
				Runtime->DestroyComponent(Chunk.CollisionComponent_GameThread);
				Runtime->DestroyComponent(Chunk.NavigationComponent_GameThread);
			}
			else
			{
				Chunk.CollisionComponent_GameThread.Reset();
				Chunk.NavigationComponent_GameThread.Reset();
			}
		}
		Chunks_RequiresLock.Empty();
	}
	ProcessChunksToDestroy();
}

void FVoxelMarchingCubeCollisionExecNodeRuntime::Tick(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(!IsDestroyed());

	if (!IsGameWorld() &&
		!GVoxelCollisionEnableInEditor)
	{
		return;
	}

	const TSharedRef<const FVoxelInvoker> Invoker = GVoxelInvokerManager->GetInvoker(Runtime.GetWorld(), InvokerChannel_GameThread);
	if (LastInvoker_GameThread != Invoker)
	{
		LastInvoker_GameThread = Invoker;

		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, MakeWeakPtrLambda(this, [=]
		{
			Update_AnyThread(Invoker);
		}));
	}

	ProcessChunksToDestroy();
	ProcessQueuedColliders(Runtime);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMarchingCubeCollisionExecNodeRuntime::Update_AnyThread(const TSharedRef<const FVoxelInvoker>& Invoker)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	if (IsDestroyed() ||
		!ensure(Octree_RequiresLock))
	{
		return;
	}

	Octree_RequiresLock->Update(
		[&](const FOctree::FNodeRef TreeNodeRef)
		{
			const FVoxelBox NodeBounds = TreeNodeRef.GetBounds().ToVoxelBox().Scale(
				Octree_RequiresLock->VoxelSize *
				Octree_RequiresLock->ChunkSize);
			return Invoker->Intersect(NodeBounds);
		},
		[&](const FOctree::FNodeRef TreeNodeRef)
		{
			if (TreeNodeRef.GetHeight() > 0)
			{
				return;
			}

			TSharedPtr<FChunk>& Chunk = Chunks_RequiresLock.FindOrAdd(TreeNodeRef.GetMin());
			if (ensure(!Chunk))
			{
				Chunk = MakeVoxelShared<FChunk>();
			}

			const FVoxelIntBox KeyBounds = TreeNodeRef.GetBounds();
			const FVoxelBox Bounds = KeyBounds.ToVoxelBox().Scale(
				Octree_RequiresLock->VoxelSize *
				Octree_RequiresLock->ChunkSize);

			TVoxelDynamicValueFactory<FVoxelCollider> Factory(STATIC_FNAME("Marching Cube Collision"), [
				&Node = Node,
				VoxelSize = Octree_RequiresLock->VoxelSize,
				ChunkSize = Octree_RequiresLock->ChunkSize,
				Bounds](const FVoxelQuery& Query)
			{
				return Node.CreateCollider(Query, VoxelSize, ChunkSize, Bounds);
			});

			const TSharedRef<FVoxelQueryParameters> Parameters = MakeVoxelShared<FVoxelQueryParameters>();
			Parameters->Add<FVoxelLODQueryParameter>().LOD = 0;
			Chunk->Collider_RequiresLock = Factory
				.AddRef(NodeRef)
				.Priority(FVoxelTaskPriority::MakeBounds(
					Bounds,
					0,
					GetWorld(),
					GetLocalToWorld()))
				.Compute(GetContext(), Parameters);

			Chunk->Collider_RequiresLock.OnChanged(MakeWeakPtrLambda(this, [this, WeakChunk = MakeWeakPtr(Chunk)](const TSharedRef<const FVoxelCollider>& Collider)
			{
				QueuedColliders.Enqueue({ WeakChunk, Collider });
			}));
		},
		[&](const FOctree::FNodeRef TreeNodeRef)
		{
			if (TreeNodeRef.GetHeight() > 0)
			{
				return;
			}

			TSharedPtr<FChunk> Chunk;
			if (!ensure(Chunks_RequiresLock.RemoveAndCopyValue(TreeNodeRef.GetMin(), Chunk)))
			{
				return;
			}

			Chunk->Collider_RequiresLock = {};
			ChunksToDestroy.Enqueue(Chunk);
		});
}

void FVoxelMarchingCubeCollisionExecNodeRuntime::ProcessChunksToDestroy()
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedPtr<FVoxelRuntime> Runtime = GetRuntime();
	ensure(Runtime || IsDestroyed());

	TSharedPtr<FChunk> Chunk;
	while (ChunksToDestroy.Dequeue(Chunk))
	{
		if (!ensure(Chunk))
		{
			continue;
		}
		ensure(!Chunk->Collider_RequiresLock.IsValid());

		if (Runtime)
		{
			Runtime->DestroyComponent(Chunk->CollisionComponent_GameThread);
			Runtime->DestroyComponent(Chunk->NavigationComponent_GameThread);
		}
		else
		{
			Chunk->CollisionComponent_GameThread.Reset();
			Chunk->NavigationComponent_GameThread.Reset();
		}
	}
}

void FVoxelMarchingCubeCollisionExecNodeRuntime::ProcessQueuedColliders(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());
	ensure(!IsDestroyed());

	FQueuedCollider QueuedCollider;
	while (QueuedColliders.Dequeue(QueuedCollider))
	{
		const TSharedPtr<FChunk> Chunk = QueuedCollider.Chunk.Pin();
		if (!Chunk)
		{
			continue;
		}

		const TSharedRef<const FVoxelCollider> Collider = QueuedCollider.Collider.ToSharedRef();

		if (Collider->GetStruct() == FVoxelCollider::StaticStruct())
		{
			Runtime.DestroyComponent(Chunk->CollisionComponent_GameThread);
			Runtime.DestroyComponent(Chunk->NavigationComponent_GameThread);
		}
		else
		{
			UVoxelCollisionComponent* CollisionComponent = Chunk->CollisionComponent_GameThread.Get();
			if (!CollisionComponent)
			{
				CollisionComponent = Runtime.CreateComponent<UVoxelCollisionComponent>();
				Chunk->CollisionComponent_GameThread = CollisionComponent;
			}

			if (ensure(CollisionComponent))
			{
				CollisionComponent->SetRelativeLocation(Collider->GetOffset());
				CollisionComponent->SetBodyInstance(*BodyInstance_GameThread);
				CollisionComponent->SetCollider(Collider);
			}

			UVoxelNavigationComponent* NavigationComponent = Chunk->NavigationComponent_GameThread.Get();
			if (!NavigationComponent)
			{
				NavigationComponent = Runtime.CreateComponent<UVoxelNavigationComponent>();
				Chunk->NavigationComponent_GameThread = NavigationComponent;
			}

			if (ensure(NavigationComponent))
			{
				NavigationComponent->SetRelativeLocation(Collider->GetOffset());
				NavigationComponent->SetNavigationMesh(Collider->GetNavmesh());
			}
		}
	}
}