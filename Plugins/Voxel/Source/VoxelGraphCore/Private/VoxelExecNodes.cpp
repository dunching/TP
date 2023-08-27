// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelExecNodes.h"

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelRootExecuteNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	return MakeVoxelUnique<FVoxelRootExecuteNodeRuntime>(SharedThis);
}

void FVoxelRootExecuteNodeRuntime::Create()
{
	VOXEL_FUNCTION_COUNTER();

	ExecValue = GetNodeRuntime()
		.MakeDynamicValueFactory(Node.ExecInPin)
		.Thread(EVoxelTaskThread::GameThread)
		.Compute(GetContext());

	ExecValue.OnChanged_GameThread(MakeWeakPtrLambda(this, [this](const TSharedRef<const FVoxelExec>& Exec)
	{
		if (Exec->MakeRuntime)
		{
			NodeRuntime_GameThread = Exec->MakeRuntime();
		}
		else
		{
			NodeRuntime_GameThread = nullptr;
		}
	}));
}

void FVoxelRootExecuteNodeRuntime::Destroy()
{
	VOXEL_FUNCTION_COUNTER();

	ExecValue = {};
	NodeRuntime_GameThread = nullptr;
}

void FVoxelRootExecuteNodeRuntime::Tick(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();

	if (!NodeRuntime_GameThread)
	{
		return;
	}

	FVoxelQueryScope Scope(nullptr, NodeRuntime_GameThread->GetContextPtr());
	NodeRuntime_GameThread->Tick(Runtime);
}

void FVoxelRootExecuteNodeRuntime::AddReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	if (!NodeRuntime_GameThread)
	{
		return;
	}

	FVoxelQueryScope Scope(nullptr, NodeRuntime_GameThread->GetContextPtr());
	NodeRuntime_GameThread->AddReferencedObjects(Collector);
}

FVoxelOptionalBox FVoxelRootExecuteNodeRuntime::GetBounds() const
{
	VOXEL_FUNCTION_COUNTER();

	if (!NodeRuntime_GameThread)
	{
		return {};
	}

	FVoxelQueryScope Scope(nullptr, NodeRuntime_GameThread->GetContextPtr());
	return NodeRuntime_GameThread->GetBounds();
}

TVoxelFutureValue<FVoxelSpawnable> FVoxelRootExecuteNodeRuntime::GenerateSpawnable(const FVoxelMergeSpawnableRef& Ref)
{
	VOXEL_FUNCTION_COUNTER();

	if (!NodeRuntime_GameThread)
	{
		return {};
	}

	FVoxelQueryScope Scope(nullptr, NodeRuntime_GameThread->GetContextPtr());
	return NodeRuntime_GameThread->GenerateSpawnable(Ref);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_MergeExecs, Exec)
{
	const TVoxelArray<TValue<FVoxelExec>> Execs = Get(ExecsPins, Query);
	return VOXEL_ON_COMPLETE(Execs)
	{
		FVoxelExec MergedExec;
		MergedExec.MakeRuntime = [Execs]
		{
			TVoxelArray<TSharedPtr<IVoxelExecNodeRuntimeInterface>> NodeRuntimes;
			for (const TSharedRef<const FVoxelExec>& Exec : Execs)
			{
				if (!Exec->MakeRuntime)
				{
					continue;
				}

				const TSharedPtr<IVoxelExecNodeRuntimeInterface> NodeRuntime = Exec->MakeRuntime();
				if (!NodeRuntime)
				{
					continue;
				}

				NodeRuntimes.Add(NodeRuntime);
			}

			return MakeVoxelShared<FVoxelMergeExecsNodeRuntime>(NodeRuntimes);
		};
		return MergedExec;
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMergeExecsNodeRuntime::Tick(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();

	for (const TSharedPtr<IVoxelExecNodeRuntimeInterface>& NodeRuntime : NodeRuntimes)
	{
		FVoxelQueryScope Scope(nullptr, NodeRuntime->GetContextPtr());
		NodeRuntime->Tick(Runtime);
	}
}

void FVoxelMergeExecsNodeRuntime::AddReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	for (const TSharedPtr<IVoxelExecNodeRuntimeInterface>& NodeRuntime : NodeRuntimes)
	{
		FVoxelQueryScope Scope(nullptr, NodeRuntime->GetContextPtr());
		NodeRuntime->AddReferencedObjects(Collector);
	}
}

FVoxelOptionalBox FVoxelMergeExecsNodeRuntime::GetBounds() const
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelOptionalBox Bounds;
	for (const TSharedPtr<IVoxelExecNodeRuntimeInterface>& NodeRuntime : NodeRuntimes)
	{
		FVoxelQueryScope Scope(nullptr, NodeRuntime->GetContextPtr());
		Bounds += NodeRuntime->GetBounds();
	}
	return Bounds;
}

TVoxelFutureValue<FVoxelSpawnable> FVoxelMergeExecsNodeRuntime::GenerateSpawnable(const FVoxelMergeSpawnableRef& Ref)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelFutureValue<FVoxelSpawnable> Result;
	for (const TSharedPtr<IVoxelExecNodeRuntimeInterface>& NodeRuntime : NodeRuntimes)
	{
		FVoxelQueryScope Scope(nullptr, NodeRuntime->GetContextPtr());

		const TVoxelFutureValue<FVoxelSpawnable> Spawnable = NodeRuntime->GenerateSpawnable(Ref);
		if (!Spawnable.IsValid())
		{
			continue;
		}

		ensure(!Result.IsValid());
		Result = Spawnable;
	}
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_DebugDelay, Exec)
{
	const TVoxelArray<TValue<FVoxelExec>> Execs = Get(ExecsPins, Query);
	const TValue<float> Delay = Get(DelayPin, Query);

	return VOXEL_ON_COMPLETE(Execs, Delay)
	{
		const FVoxelDummyFutureValue Dummy = FVoxelFutureValue::MakeDummy();

		FVoxelSystemUtilities::DelayedCall([Dummy]
		{
			Dummy.MarkDummyAsCompleted();
		}, Delay);

		return VOXEL_ON_COMPLETE(Execs, Dummy)
		{
			FVoxelExec MergedExec;
			MergedExec.MakeRuntime = [Execs]
			{
				TVoxelArray<TSharedPtr<IVoxelExecNodeRuntimeInterface>> NodeRuntimes;
				for (const TSharedRef<const FVoxelExec>& Exec : Execs)
				{
					if (!Exec->MakeRuntime)
					{
						continue;
					}

					const TSharedPtr<IVoxelExecNodeRuntimeInterface> NodeRuntime = Exec->MakeRuntime();
					if (!NodeRuntime)
					{
						continue;
					}

					NodeRuntimes.Add(NodeRuntime);
				}

				return MakeVoxelShared<FVoxelMergeExecsNodeRuntime>(NodeRuntimes);
			};
			return MergedExec;
		};
	};
}