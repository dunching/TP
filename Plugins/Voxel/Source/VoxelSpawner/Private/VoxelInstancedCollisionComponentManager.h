// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelInstancedCollisionComponent.h"

class FVoxelInstancedCollisionComponentManager;

extern FVoxelInstancedCollisionComponentManager* GVoxelInstancedCollisionComponentTicker;

class FVoxelInstancedCollisionComponentManager : public FVoxelTicker
{
public:
	FVoxelInstancedCollisionComponentManager() = default;

	//~ Begin FVoxelTicker Interface
	virtual void Tick() override;
	//~ End FVoxelTicker Interface

	void UpdateComponent(UVoxelInstancedCollisionComponent* Component);

private:
	class FComponentUpdater : public TSharedFromThis<FComponentUpdater>
	{
	public:
		const TWeakObjectPtr<UVoxelInstancedCollisionComponent> WeakComponent;
		const FName InvokerChannel;
		const TSharedRef<const FVoxelInvoker> Invoker;
		const TSharedRef<const FVoxelMeshDataBase> MeshData;
		const bool bAllInstancesEnabled;
		const TSharedPtr<const TVoxelAddOnlySet<int32>> OldEnabledInstances;

		FComponentUpdater(
			UVoxelInstancedCollisionComponent& Component,
			const FName InvokerChannel,
			const TSharedRef<const FVoxelInvoker>& Invoker)
			: WeakComponent(&Component)
			, InvokerChannel(InvokerChannel)
			, Invoker(Invoker)
			, MeshData(Component.MeshData.ToSharedRef())
			, bAllInstancesEnabled(Component.bAllInstancesEnabled)
			, OldEnabledInstances(Component.EnabledInstances)
		{
		}

		void Update_AnyThread();
		void Finalize_GameThread();

	private:
		TVoxelAddOnlySet<int32> NewEnabledInstances;
		TVoxelAddOnlySet<int32> InstancesToEnable;
		TVoxelAddOnlySet<int32> InstancesToDisable;
	};

	class FChannel : public TSharedFromThis<FChannel>
	{
	public:
		const FObjectKey World;
		const FName InvokerChannel;

		bool bUpdateQueued = false;
		bool bUpdateProcessing = false;

		TSharedPtr<const FVoxelInvoker> Invoker_GameThread;
		TVoxelSet<TWeakObjectPtr<UVoxelInstancedCollisionComponent>> Components;

		FChannel(const FObjectKey World, const FName InvokerChannel)
			: World(World)
			, InvokerChannel(InvokerChannel)
		{
		}

		void Tick();
	};

	TVoxelMap<FName, TSharedPtr<FChannel>> ChannelToData;
};