// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "MarchingCube/VoxelMarchingCubeExecNode.h"
#include "MarchingCube/VoxelMarchingCubeNodes.h"
#include "MarchingCube/VoxelMarchingCubeMesh.h"
#include "VoxelRuntime.h"
#include "VoxelGradientNodes.h"
#include "VoxelDetailTextureNodes.h"
#include "VoxelScreenSizeChunkSpawner.h"
#include "Rendering/VoxelMeshComponent.h"

FVoxelNodeAliases::TValue<FVoxelMarchingCubeExecNodeMesh> FVoxelMarchingCubeExecNode::CreateMesh(
	const FVoxelQuery& InQuery,
	const float VoxelSize,
	const int32 ChunkSize,
	const FVoxelBox& Bounds) const
{
	checkVoxelSlow(FVoxelTaskReferencer::Get().IsReferenced(this));
	const FVoxelQuery Query = InQuery.EnterScope(*this);

	const TValue<FVoxelMarchingCubeSurface> MarchingCubeSurface = VOXEL_CALL_NODE(FVoxelNode_GenerateMarchingCubeSurface, SurfacePin, Query)
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
			return true;
		};
		VOXEL_CALL_NODE_BIND(PerfectTransitionsPin)
		{
			return GetNodeRuntime().Get(PerfectTransitionsPin, Query);
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

	const TValue<FVoxelMesh> Mesh = VOXEL_CALL_NODE(FVoxelNode_CreateMarchingCubeMesh, MeshPin, Query)
	{
		VOXEL_CALL_NODE_BIND(SurfacePin, MarchingCubeSurface)
		{
			return MarchingCubeSurface;
		};

		VOXEL_CALL_NODE_BIND(MaterialPin)
		{
			FindVoxelQueryParameter(FVoxelLODQueryParameter, LODQueryParameter);
			const int32 LOD = LODQueryParameter->LOD;

			const TValue<FVoxelMaterial> Material = GetNodeRuntime().Get(MaterialPin, Query);

			const TValue<FVoxelMaterialParameter> MaterialIdMaterialParameter = VOXEL_CALL_NODE(FVoxelNode_MakeMaterialIdDetailTextureParameter, ParameterPin, Query)
			{
				CalleeNode.bIsMain = true;

				VOXEL_CALL_NODE_BIND(SurfacePin)
				{
					return GetNodeRuntime().Get(SurfacePin, Query);
				};
				VOXEL_CALL_NODE_BIND(TexturePin)
				{
					return GetNodeRuntime().Get(MaterialIdDetailTexturePin, Query);
				};
			};

			if (LOD == 0)
			{
				return VOXEL_ON_COMPLETE(LOD, Material, MaterialIdMaterialParameter)
				{
					FVoxelMaterial MaterialCopy = *Material;
					MaterialCopy.Parameters.Add(MaterialIdMaterialParameter);
					return MaterialCopy;
				};
			}

			const TValue<FVoxelMaterialParameter> NormalMaterialParameter = VOXEL_CALL_NODE(FVoxelNode_MakeNormalDetailTextureParameter, ParameterPin, Query)
			{
				CalleeNode.bIsMain = true;

				VOXEL_CALL_NODE_BIND(NormalPin)
				{
					return VOXEL_CALL_NODE(FVoxelNode_GetGradient, GradientPin, Query)
					{
						VOXEL_CALL_NODE_BIND(ValuePin)
						{
							const TValue<FVoxelSurface> Surface = GetNodeRuntime().Get(SurfacePin, Query);
							return VOXEL_ON_COMPLETE(Surface)
							{
								return Surface->GetDistance(Query);
							};
						};
					};
				};
				VOXEL_CALL_NODE_BIND(TexturePin)
				{
					return GetNodeRuntime().Get(NormalDetailTexturePin, Query);
				};
			};

			return VOXEL_ON_COMPLETE(LOD, Material, MaterialIdMaterialParameter, NormalMaterialParameter)
			{
				FVoxelMaterial MaterialCopy = *Material;
				MaterialCopy.Parameters.Add(MaterialIdMaterialParameter);
				MaterialCopy.Parameters.Add(NormalMaterialParameter);
				return MaterialCopy;
			};
		};

		VOXEL_CALL_NODE_BIND(DistancePin)
		{
			const TValue<FVoxelSurface> Surface = GetNodeRuntime().Get(SurfacePin, Query);
			return VOXEL_ON_COMPLETE(Surface)
			{
				return Surface->GetDistance(Query);
			};
		};

		VOXEL_CALL_NODE_BIND(GenerateDistanceFieldPin)
		{
			return GetNodeRuntime().Get(GenerateDistanceFieldsPin, Query);
		};

		VOXEL_CALL_NODE_BIND(DistanceFieldBiasPin)
		{
			return GetNodeRuntime().Get(DistanceFieldBiasPin, Query);
		};
	};

	const TValue<FVoxelMeshSettings> MeshSettings = GetNodeRuntime().Get(MeshSettingsPin, Query);
	const TValue<FBodyInstance> BodyInstance = GetNodeRuntime().Get(BodyInstancePin, Query);

	return
		MakeVoxelTask(STATIC_FNAME("MarchingCubeSceneNode - CreateCollider"))
		.Dependencies(Mesh, MeshSettings, BodyInstance)
		.Execute<FVoxelMarchingCubeExecNodeMesh>([=]() -> TValue<FVoxelMarchingCubeExecNodeMesh>
		{
			const TSharedRef<FVoxelMarchingCubeExecNodeMesh> Result = MakeVoxelShared<FVoxelMarchingCubeExecNodeMesh>();
			if (Mesh.Get_CheckCompleted().GetStruct() == FVoxelMesh::StaticStruct())
			{
				return Result;
			}
			Result->Mesh = Mesh.GetShared_CheckCompleted();
			Result->MeshSettings = MeshSettings.GetShared_CheckCompleted();

			if (Query.GetInfo(EVoxelQueryInfo::Query).FindParameter<FVoxelRuntimeParameter_DisableCollision>() ||
				BodyInstance.Get_CheckCompleted().GetCollisionEnabled(false) == ECollisionEnabled::NoCollision)
			{
				return Result;
			}

			const TValue<FVoxelCollider> Collider = VOXEL_CALL_NODE(FVoxelNode_CreateMarchingCubeCollider, ColliderPin, Query)
			{
				VOXEL_CALL_NODE_BIND(SurfacePin, MarchingCubeSurface)
				{
					return MarchingCubeSurface;
				};

				VOXEL_CALL_NODE_BIND(PhysicalMaterialPin)
				{
					return GetNodeRuntime().Get(PhysicalMaterialPin, Query);
				};
			};

			return
				MakeVoxelTask(STATIC_FNAME("MarchingCubeSceneNode - Create"))
				.Dependencies(Collider)
				.Execute<FVoxelMarchingCubeExecNodeMesh>([=]
				{
					if (Collider.Get_CheckCompleted().GetStruct() == FVoxelCollider::StaticStruct())
					{
						// If mesh isn't empty collider shouldn't be either
						ensure(false);
						return Result;
					}

					Result->Collider = Collider.GetShared_CheckCompleted();
					Result->BodyInstance = BodyInstance.GetShared_CheckCompleted();
					return Result;
				});
		});
}

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelMarchingCubeExecNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	if (!FApp::CanEverRender())
	{
		// Never create on server
		return nullptr;
	}

	return MakeVoxelUnique<FVoxelMarchingCubeExecNodeRuntime>(SharedThis);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMarchingCubeExecNodeRuntime::Create()
{
	VOXEL_FUNCTION_COUNTER();

	ChunkSpawnerValue = GetNodeRuntime().MakeDynamicValueFactory(Node.ChunkSpawnerPin).Compute(GetContext());
	ChunkSpawnerValue.OnChanged_GameThread(MakeWeakPtrLambda(this, [this](const TSharedRef<const FVoxelChunkSpawner>& NewSpawner)
	{
		ChunkSpawner = NewSpawner->MakeImpl();

		if (!ChunkSpawner)
		{
			ChunkSpawner = FVoxelScreenSizeChunkSpawner().MakeImpl();
		}

		ChunkSpawner->VoxelSize = VoxelSize;
		ChunkSpawner->CreateChunkLambda = MakeWeakPtrLambda(this, [this](
			const int32 LOD,
			const int32 ChunkSize,
			const FVoxelBox& Bounds,
			TSharedPtr<FVoxelChunkRef>& OutChunkRef)
		{
			const TSharedRef<FChunkInfo> ChunkInfo = MakeVoxelShared<FChunkInfo>(LOD, ChunkSize, Bounds);

			VOXEL_SCOPE_LOCK(ChunkInfos_CriticalSection);
			ChunkInfos.Add(ChunkInfo->ChunkId, ChunkInfo);

			OutChunkRef = MakeVoxelShared<FVoxelChunkRef>(ChunkInfo->ChunkId, ChunkActionQueue);
		});
	}));

	VoxelSizeValue = GetNodeRuntime().MakeDynamicValueFactory(Node.VoxelSizePin).Compute(GetContext());
	VoxelSizeValue.OnChanged_GameThread(MakeWeakPtrLambda(this, [this](const float NewVoxelSize)
	{
		if (VoxelSize == NewVoxelSize)
		{
			return;
		}

		VoxelSize = NewVoxelSize;

		if (!ChunkSpawner)
		{
			return;
		}

		ChunkSpawner->VoxelSize = NewVoxelSize;
		ChunkSpawner->Recreate();
	}));
}

void FVoxelMarchingCubeExecNodeRuntime::Destroy()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(ChunkInfos_CriticalSection);

	const TSharedPtr<FVoxelRuntime> Runtime = GetRuntime();
	if (!Runtime)
	{
		// Already destroyed
		for (const auto& It : ChunkInfos)
		{
			It.Value->Mesh = {};
			It.Value->MeshComponent = {};
			It.Value->CollisionComponent = {};
			It.Value->FlushOnComplete();
		}
		ChunkInfos.Empty();
		return;
	}

	TArray<FVoxelChunkId> ChunkIds;
	ChunkInfos.GenerateKeyArray(ChunkIds);

	for (const FVoxelChunkId ChunkId : ChunkIds)
	{
		ProcessAction(&*Runtime, FVoxelChunkAction(EVoxelChunkAction::Destroy, ChunkId));
	}

	ensure(ChunkInfos.Num() == 0);
}

void FVoxelMarchingCubeExecNodeRuntime::Tick(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();

	if (ChunkSpawnerValue.IsComputed() &&
		VoxelSizeValue.IsComputed() &&
		ChunkSpawner)
	{
		ChunkSpawner->Tick(Runtime);
	}

	ProcessMeshes(Runtime);
	ProcessActions(&Runtime, true);

	if (ProcessActionsGraphEvent.IsValid())
	{
		VOXEL_SCOPE_COUNTER("Wait");
		ProcessActionsGraphEvent->Wait();
	}
	ProcessActionsGraphEvent = TGraphTask<TVoxelGraphTask<ENamedThreads::AnyBackgroundThreadNormalTask>>::CreateTask().ConstructAndDispatchWhenReady(MakeWeakPtrLambda(this, [this]
	{
		ProcessActions(nullptr, false);
	}));
}

void FVoxelMarchingCubeExecNodeRuntime::FChunkInfo::FlushOnComplete()
{
	if (OnCompleteArray.Num() == 0)
	{
		return;
	}

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [OnCompleteArray = MoveTemp(OnCompleteArray)]
	{
		for (const TSharedPtr<const TVoxelUniqueFunction<void()>>& OnComplete : OnCompleteArray)
		{
			(*OnComplete)();
		}
	});
}

void FVoxelMarchingCubeExecNodeRuntime::ProcessMeshes(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();

	TArray<TSharedPtr<const TVoxelUniqueFunction<void()>>> OnCompleteArray;
	ON_SCOPE_EXIT
	{
		if (OnCompleteArray.Num() > 0)
		{
			AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [OnCompleteArray = MoveTemp(OnCompleteArray)]
			{
				for (const TSharedPtr<const TVoxelUniqueFunction<void()>>& OnComplete : OnCompleteArray)
				{
					(*OnComplete)();
				}
			});
		}
	};

	VOXEL_SCOPE_LOCK(ChunkInfos_CriticalSection);

	FQueuedMesh QueuedMesh;
	while (QueuedMeshes->Dequeue(QueuedMesh))
	{
		const TSharedPtr<FChunkInfo> ChunkInfo = ChunkInfos.FindRef(QueuedMesh.ChunkId);
		if (!ChunkInfo)
		{
			continue;
		}

		const TSharedPtr<const FVoxelMesh> Mesh = QueuedMesh.Mesh->Mesh;
		const TSharedPtr<const FVoxelCollider> Collider = QueuedMesh.Mesh->Collider;

		if (!Mesh)
		{
			Runtime.DestroyComponent(ChunkInfo->MeshComponent);
		}
		else
		{
			ENQUEUE_RENDER_COMMAND(SetTransitionMask_RenderThread)([Mesh, TransitionMask = ChunkInfo->TransitionMask](FRHICommandList& RHICmdList)
			{
				ConstCast(CastChecked<FVoxelMarchingCubeMesh>(*Mesh)).SetTransitionMask_RenderThread(RHICmdList, TransitionMask);
			});

			if (!ChunkInfo->MeshComponent.IsValid())
			{
				ChunkInfo->MeshComponent = Runtime.CreateComponent<UVoxelMeshComponent>();
			}

			UVoxelMeshComponent* Component = ChunkInfo->MeshComponent.Get();
			if (ensure(Component))
			{
				Component->SetRelativeLocation(ChunkInfo->Bounds.Min);
				Component->SetMesh(Mesh);
				QueuedMesh.Mesh->MeshSettings->ApplyToComponent(*Component);
			}

            const auto Box = ChunkInfo->Bounds.ToFBox();
            for (const auto& Iter : Runtime.OnChunkChangedMap)
            {
                if (Iter.Value)
                {
                    Iter.Value(
                        Box,
                        ChunkInfo->LOD,
                        ChunkInfo->ChunkSize
                    );
                }
            }
		}

		if (!Collider)
		{
			Runtime.DestroyComponent(ChunkInfo->CollisionComponent);
		}
		else
		{
			if (!ChunkInfo->CollisionComponent.IsValid())
			{
				ChunkInfo->CollisionComponent = Runtime.CreateComponent<UVoxelCollisionComponent>();
			}

			UVoxelCollisionComponent* Component = ChunkInfo->CollisionComponent.Get();
			if (ensure(Component))
			{
				Component->SetRelativeLocation(Collider->GetOffset());
				Component->SetBodyInstance(*QueuedMesh.Mesh->BodyInstance);
				if (!IsGameWorld())
				{
					Component->BodyInstance.SetResponseToChannel(ECC_EngineTraceChannel6, ECR_Block);
				}
				Component->SetCollider(Collider);
			}
		}

		OnCompleteArray.Append(ChunkInfo->OnCompleteArray);
		ChunkInfo->OnCompleteArray.Empty();
	}
}

void FVoxelMarchingCubeExecNodeRuntime::ProcessActions(FVoxelRuntime* Runtime, const bool bIsInGameThread)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(ChunkInfos_CriticalSection);
	ensure(bIsInGameThread == IsInGameThread());
	ensure(bIsInGameThread == (Runtime != nullptr));

	const double StartTime = FPlatformTime::Seconds();

	FVoxelChunkAction Action;
	while ((bIsInGameThread ? ChunkActionQueue->GameQueue : ChunkActionQueue->AsyncQueue).Dequeue(Action))
	{
		ProcessAction(Runtime, Action);

		if (FPlatformTime::Seconds() - StartTime > 0.1f)
		{
			break;
		}
	}
}

void FVoxelMarchingCubeExecNodeRuntime::ProcessAction(FVoxelRuntime* Runtime, const FVoxelChunkAction& Action)
{
	checkVoxelSlow(ChunkInfos_CriticalSection.IsLocked_Debug());

	const TSharedPtr<FChunkInfo> ChunkInfo = ChunkInfos.FindRef(Action.ChunkId);
	if (!ChunkInfo)
	{
		return;
	}

	switch (Action.Action)
	{
	default: ensure(false);
	case EVoxelChunkAction::Compute:
	{
		VOXEL_SCOPE_COUNTER("Compute");

		TVoxelDynamicValueFactory<FVoxelMarchingCubeExecNodeMesh> Factory(STATIC_FNAME("Marching Cube Mesh"), [
			&Node = Node,
			VoxelSize = VoxelSize,
			ChunkSize = ChunkInfo->ChunkSize,
			Bounds = ChunkInfo->Bounds](const FVoxelQuery& Query)
		{
			checkVoxelSlow(FVoxelTaskReferencer::Get().IsReferenced(&Node));
			return Node.CreateMesh(Query, VoxelSize, ChunkSize, Bounds);
		});

		const TSharedRef<FVoxelQueryParameters> Parameters = MakeVoxelShared<FVoxelQueryParameters>();
		Parameters->Add<FVoxelLODQueryParameter>().LOD = ChunkInfo->LOD;

		ensure(!ChunkInfo->Mesh.IsValid());
		ChunkInfo->Mesh = Factory
			.AddRef(NodeRef)
			.Priority(FVoxelTaskPriority::MakeBounds(
				ChunkInfo->Bounds,
				0,
				GetWorld(),
				GetLocalToWorld()))
			.Compute(GetContext(), Parameters);

		ChunkInfo->Mesh.OnChanged([QueuedMeshes = QueuedMeshes, ChunkId = ChunkInfo->ChunkId](const TSharedRef<const FVoxelMarchingCubeExecNodeMesh>& NewMesh)
		{
			QueuedMeshes->Enqueue(FQueuedMesh{ ChunkId, NewMesh });
		});

		if (Action.OnComputeComplete)
		{
			ChunkInfo->OnCompleteArray.Add(Action.OnComputeComplete);
		}
	}
	break;
	case EVoxelChunkAction::SetTransitionMask:
	{
		VOXEL_SCOPE_COUNTER("SetTransitionMask");
		check(IsInGameThread());

 		ChunkInfo->TransitionMask = Action.TransitionMask;
 
 		if (UVoxelMeshComponent* Component = ChunkInfo->MeshComponent.Get())
 		{
 			if (const TSharedPtr<const FVoxelMesh> Mesh = Component->GetMesh())
 			{
 				ConstCast(CastChecked<FVoxelMarchingCubeMesh>(*Mesh)).SetTransitionMask_GameThread(Action.TransitionMask);
 			}
 		}
	}
	break;
	case EVoxelChunkAction::BeginDestroy:
	{
		VOXEL_SCOPE_COUNTER("BeginDestroy");

		ChunkInfo->Mesh = {};
	}
	break;
	case EVoxelChunkAction::Destroy:
	{
		VOXEL_SCOPE_COUNTER("Destroy");
		check(IsInGameThread());
		check(Runtime);

		ChunkInfo->Mesh = {};

		Runtime->DestroyComponent(ChunkInfo->MeshComponent);
		Runtime->DestroyComponent(ChunkInfo->CollisionComponent);

		ChunkInfo->FlushOnComplete();

		ChunkInfos.Remove(Action.ChunkId);

		ensure(ChunkInfo.GetSharedReferenceCount() == 1);
	}
	break;
	}
}