// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelExecNode.h"
#include "VoxelRuntime.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelExecNodeRuntime);

DEFINE_VOXEL_NODE_COMPUTE(FVoxelExecNode, Exec)
{
	const TValue<bool> EnableNode = Get(EnableNodePin, Query);

	return VOXEL_ON_COMPLETE(EnableNode)
	{
		if (!EnableNode)
		{
			return {};
		}

		const TSharedRef<FVoxelExecNode> This = SharedNode<FVoxelExecNode>();

		FVoxelExec Exec;
		Exec.MakeRuntime = [This, Context = Query.GetSharedContext(), NodeId = GetNodeRef().NodeId]() -> TSharedPtr<FVoxelExecNodeRuntime>
		{
			const TSharedPtr<FVoxelExecNodeRuntime> NodeRuntime = This->CreateSharedExecRuntime(This);
			if (!NodeRuntime)
			{
				return nullptr;
			}

			return Context->FindOrAddExecNodeRuntime(NodeId, NodeRuntime.ToSharedRef());
		};
		return Exec;
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FVoxelExecNodeRuntime> FVoxelExecNode::CreateSharedExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	TVoxelUniquePtr<FVoxelExecNodeRuntime> ExecRuntime = CreateExecRuntime(SharedThis);
	if (!ExecRuntime)
	{
		return nullptr;
	}

	return TSharedPtr<FVoxelExecNodeRuntime>(ExecRuntime.Release(), [](FVoxelExecNodeRuntime* Runtime)
	{
		if (!Runtime)
		{
			return;
		}

		FVoxelUtilities::RunOnGameThread([Runtime]
		{
			if (Runtime->bIsCreated)
			{
				Runtime->CallDestroy();
			}
			Runtime->~FVoxelExecNodeRuntime();
			FVoxelMemory::Free(Runtime);
		});
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelExecNodeRuntime::~FVoxelExecNodeRuntime()
{
	ensure(!bIsCreated || bIsDestroyed);
}

TSharedPtr<FVoxelRuntime> FVoxelExecNodeRuntime::GetRuntime() const
{
	const TSharedPtr<FVoxelRuntime> Runtime = PrivateContext->RuntimeInfo->GetRuntime();
	ensure(Runtime || IsDestroyed());
	return Runtime;
}

USceneComponent* FVoxelExecNodeRuntime::GetRootComponent() const
{
	ensure(IsInGameThread());
	USceneComponent* RootComponent = PrivateContext->RuntimeInfo->GetRootComponent();
	ensure(RootComponent);
	return RootComponent;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelExecNodeRuntime::CallCreate(const TSharedRef<FVoxelQueryContext>& Context)
{
	VOXEL_FUNCTION_COUNTER();

	ensure(!bIsCreated);
	bIsCreated = true;

	PrivateContext = Context->EnterScope(NodeRef->GetNodeRef());

	PreCreate();
	Create();
}

void FVoxelExecNodeRuntime::CallDestroy()
{
	VOXEL_FUNCTION_COUNTER();

	ensure(bIsCreated);
	ensure(!bIsDestroyed);
	bIsDestroyed = true;

	Destroy();
}