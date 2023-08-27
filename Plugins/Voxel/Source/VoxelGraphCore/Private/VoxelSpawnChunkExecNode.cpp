// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSpawnChunkExecNode.h"

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelSpawnChunkExecNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	return MakeVoxelUnique<FVoxelSpawnChunkExecNodeRuntime>(SharedThis);
}

void FVoxelSpawnChunkExecNodeRuntime::Create()
{
	VOXEL_FUNCTION_COUNTER();

	const FVoxelMergeSpawnableRef MergeSpawnableRef(*GetContext());

	TVoxelDynamicValueFactory<FVoxelSpawnable> Factory(STATIC_FNAME("SpawnChunk"), [&Node = Node, MergeSpawnableRef](const FVoxelQuery& Query)
	{
		const TValue<FVoxelBox> FutureBounds = Node.GetNodeRuntime().Get(Node.BoundsPin, Query);

		return
			MakeVoxelTask()
			.Dependency(FutureBounds)
			.Execute<FVoxelSpawnable>([=, &Node]
			{
				const TSharedRef<FVoxelQueryParameters> Parameters = MakeVoxelShared<FVoxelQueryParameters>();
				Parameters->Add<FVoxelSpawnableBoundsQueryParameter>().Bounds = FutureBounds.Get_CheckCompleted();
				Parameters->Add<FVoxelMergeSpawnableRefQueryParameter>().MergeSpawnableRef = MergeSpawnableRef;
				return Node.GetNodeRuntime().Get(Node.SpawnablePin, Query.MakeNewQuery(Parameters));
			});
	});

	if (IsPreviewScene())
	{
		Factory.Thread(EVoxelTaskThread::GameThread);
	}

	SpawnableValue =
		Factory
		.AddRef(NodeRef)
		.Compute(GetContext());

	SpawnableValue.OnChanged(MakeWeakPtrLambda(this, [this](const TSharedRef<const FVoxelSpawnable>& NewSpawnable)
	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		if (Spawnable_RequiresLock)
		{
			Spawnable_RequiresLock->CallDestroy_AnyThread();
			Spawnable_RequiresLock.Reset();
		}

		if (NewSpawnable->IsValid())
		{
			Spawnable_RequiresLock = ConstCast(NewSpawnable);
			Spawnable_RequiresLock->CallCreate_AnyThread();

			Bounds_RequiresLock = Spawnable_RequiresLock->GetBounds();
		}

		if (FutureSpawnable_RequiresLock)
		{
			FutureSpawnable_RequiresLock->SetValue(NewSpawnable);
			FutureSpawnable_RequiresLock.Reset();
		}
	}));
}

void FVoxelSpawnChunkExecNodeRuntime::Destroy()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	SpawnableValue = {};

	VOXEL_SCOPE_LOCK(CriticalSection);

	if (Spawnable_RequiresLock)
	{
		Spawnable_RequiresLock->CallDestroy_AnyThread();
		Spawnable_RequiresLock.Reset();
	}
}

FVoxelOptionalBox FVoxelSpawnChunkExecNodeRuntime::GetBounds() const
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	return Bounds_RequiresLock;
}

TVoxelFutureValue<FVoxelSpawnable> FVoxelSpawnChunkExecNodeRuntime::GenerateSpawnable(const FVoxelMergeSpawnableRef& Ref)
{
	if (Ref.NodePath != GetContext()->Path ||
		!ensure(!Ref.NodeData.IsValid()))
	{
		return {};
	}

	VOXEL_SCOPE_LOCK(CriticalSection);

	if (Spawnable_RequiresLock)
	{
		return Spawnable_RequiresLock.ToSharedRef();
	}

	if (!FutureSpawnable_RequiresLock)
	{
		FutureSpawnable_RequiresLock = MakeVoxelShared<TVoxelFutureValueState<FVoxelSpawnable>>();
	}
	return FutureSpawnable_RequiresLock.ToSharedRef();
}