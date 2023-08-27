// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelInstancedCollisionComponentManager.h"
#include "VoxelInvoker.h"
#include "VoxelMeshDataBase.h"

FVoxelInstancedCollisionComponentManager* GVoxelInstancedCollisionComponentTicker = nullptr;

VOXEL_RUN_ON_STARTUP_GAME(RegisterVoxelInstancedCollisionComponentTicker)
{
	GVoxelInstancedCollisionComponentTicker = new FVoxelInstancedCollisionComponentManager();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstancedCollisionComponentManager::Tick()
{
	VOXEL_FUNCTION_COUNTER();

	for (auto It = ChannelToData.CreateIterator(); It; ++It)
	{
		FChannel& Channel = *It.Value();
		if (!Channel.World.ResolveObjectPtr() ||
			Channel.Components.Num() == 0)
		{
			It.RemoveCurrent();
			continue;
		}

		Channel.Tick();
	}
}

void FVoxelInstancedCollisionComponentManager::UpdateComponent(UVoxelInstancedCollisionComponent* Component)
{
	if (Component->InvokerChannel.IsNone())
	{
		return;
	}

	TSharedPtr<FChannel>& Channel = ChannelToData.FindOrAdd(Component->InvokerChannel);
	if (!Channel)
	{
		Channel = MakeVoxelShared<FChannel>(Component->GetWorld(), Component->InvokerChannel);
	}
	Channel->Components.Add(Component);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstancedCollisionComponentManager::FComponentUpdater::Update_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();

	if (OldEnabledInstances)
	{
		NewEnabledInstances.Reserve(OldEnabledInstances->Num());
	}

	{
		VOXEL_SCOPE_COUNTER("Traverse");

		MeshData->GetTree()->Traverse(
			[&](const FVoxelBox& NodeBounds)
			{
				return Invoker->Intersect(NodeBounds);
			},
			[&](const int32 Index)
			{
				NewEnabledInstances.Add(Index);
			});
	}

	if (!bAllInstancesEnabled)
	{
		if (OldEnabledInstances)
		{
			VOXEL_SCOPE_COUNTER("InstancesToEnable");

			InstancesToEnable.Reserve(NewEnabledInstances.Num());
			for (const int32 Index : NewEnabledInstances)
			{
				if (!OldEnabledInstances->Contains(Index))
				{
					InstancesToEnable.Add(Index);
				}
			}
		}
		else
		{
			InstancesToEnable = NewEnabledInstances;
		}
	}

	if (bAllInstancesEnabled)
	{
		VOXEL_SCOPE_COUNTER("InstancesToDisable");
		InstancesToDisable.Reserve(MeshData->Num());

		for (int32 Index = 0; Index < MeshData->Num(); Index++)
		{
			if (!NewEnabledInstances.Contains(Index))
			{
				InstancesToDisable.Add(Index);
			}
		}
	}
	else if (OldEnabledInstances)
	{
		VOXEL_SCOPE_COUNTER("InstancesToDisable");
		InstancesToDisable.Reserve(OldEnabledInstances->Num());

		for (const int32 Index : *OldEnabledInstances)
		{
			if (!NewEnabledInstances.Contains(Index))
			{
				InstancesToDisable.Add(Index);
			}
		}
	}
}

void FVoxelInstancedCollisionComponentManager::FComponentUpdater::Finalize_GameThread()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	UVoxelInstancedCollisionComponent* Component = WeakComponent.Get();
	if (!Component)
	{
		return;
	}

	if (Component->MeshData != MeshData ||
		Component->InvokerChannel != InvokerChannel	 ||
		!ensure(Component->bAllInstancesEnabled == bAllInstancesEnabled) ||
		!ensure(Component->EnabledInstances == OldEnabledInstances))
	{
		return;
	}

	UBodySetup* BodySetup = Component->GetBodySetup();
	FPhysScene* PhysicsScene = Component->GetWorld()->GetPhysicsScene();
	if (!ensure(BodySetup) ||
		!ensure(PhysicsScene))
	{
		return;
	}

	Component->EnabledInstances = MakeSharedCopy(MoveTemp(NewEnabledInstances));
	Component->bAllInstancesEnabled = false;

	TCompatibleVoxelArray<FBodyInstance*> ValidBodyInstances;
	TCompatibleVoxelArray<FTransform> ValidTransforms;
	ValidBodyInstances.Reserve(InstancesToEnable.Num());
	ValidTransforms.Reserve(InstancesToEnable.Num());

	for (const int32 Index : InstancesToEnable)
	{
		if (!ensure(Component->InstanceBodies.IsValidIndex(Index)))
		{
			continue;
		}

		TVoxelUniquePtr<FBodyInstance>& Body = Component->InstanceBodies[Index];
		if (!ensure(!Body))
		{
			continue;
		}

		const FTransform InstanceTransform = FTransform(MeshData->Transforms[Index]) * Component->GetComponentTransform();
		if (InstanceTransform.GetScale3D().IsNearlyZero())
		{
			continue;
		}

		Body = Component->MakeBodyInstance(Index);

		ValidBodyInstances.Add(Body.Get());
		ValidTransforms.Add(InstanceTransform);
	}

	for (const int32 Index : InstancesToDisable)
	{
		if (!ensure(Component->InstanceBodies.IsValidIndex(Index)))
		{
			continue;
		}

		TVoxelUniquePtr<FBodyInstance>& Body = Component->InstanceBodies[Index];
		if (!Body)
		{
			ensure(MeshData->Transforms[Index].GetScale3D().IsNearlyZero());
			continue;
		}

		Body->TermBody();
		Body.Reset();
	}

	if (ValidBodyInstances.Num() > 0)
	{
		FBodyInstance::InitStaticBodies(ValidBodyInstances, ValidTransforms, BodySetup, Component, PhysicsScene);
		Component->MarkRenderStateDirty();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstancedCollisionComponentManager::FChannel::Tick()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	const TSharedRef<const FVoxelInvoker> NewInvoker = GVoxelInvokerManager->GetInvoker(World, InvokerChannel);
	if (Invoker_GameThread != NewInvoker)
	{
		bUpdateQueued = true;
		Invoker_GameThread = NewInvoker;
	}

	// Remove old components
	for (auto It = Components.CreateIterator(); It; ++It)
	{
		UVoxelInstancedCollisionComponent* Component = (*It).Get();
		if (!Component ||
			!Component->HasMeshData())
		{
			It.RemoveCurrent();
			continue;
		}

		if (Component->InvokerChannel != InvokerChannel)
		{
			if (Component->InvokerChannel.IsNone())
			{
				Component->CreateAllBodies();
			}

			It.RemoveCurrent();
			continue;
		}

		ensure(Component);
	}

	if (!bUpdateQueued ||
		bUpdateProcessing)
	{
		return;
	}

	ensure(bUpdateQueued);
	bUpdateQueued = false;

	ensure(!bUpdateProcessing);
	bUpdateProcessing = true;

	TVoxelArray<TSharedRef<FComponentUpdater>> ComponentUpdaters;
	for (const TWeakObjectPtr<UVoxelInstancedCollisionComponent> Component : Components)
	{
		ComponentUpdaters.Add(MakeVoxelShared<FComponentUpdater>(*Component, InvokerChannel, Invoker_GameThread.ToSharedRef()));
	}

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, MakeWeakPtrLambda(this, [=]
	{
		VOXEL_SCOPE_COUNTER_FORMAT("Update collision InvokerChannel=%s", *InvokerChannel.ToString());

		for (const TSharedRef<FComponentUpdater>& ComponentUpdater : ComponentUpdaters)
		{
			ComponentUpdater->Update_AnyThread();
		}

		FVoxelUtilities::RunOnGameThread(MakeWeakPtrLambda(this, [=]
		{
			ensure(bUpdateProcessing);
			bUpdateProcessing = false;

			for (const TSharedRef<FComponentUpdater>& ComponentUpdater : ComponentUpdaters)
			{
				ComponentUpdater->Finalize_GameThread();
			}
		}));
	}));
}