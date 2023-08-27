// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SculptVolume/VoxelSetSculptVolumeSourceSurfaceExecNode.h"
#include "SculptVolume/VoxelSculptVolumeStorage.h"
#include "SculptVolume/VoxelSculptVolumeStorageData.h"
#include "VoxelDependency.h"

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelSetSculptVolumeSourceSurfaceExecNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	return MakeVoxelUnique<FVoxelSetSculptVolumeSourceSurfaceExecNodeRuntime>(SharedThis);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSetSculptVolumeSourceSurfaceExecNodeRuntime::Create()
{
	VOXEL_FUNCTION_COUNTER();

	if (IsPreviewScene())
	{
		return;
	}

	const TSharedPtr<const FVoxelRuntimeParameter_SculptVolumeStorage> Parameter = FindParameter<FVoxelRuntimeParameter_SculptVolumeStorage>();
	if (!Parameter)
	{
		VOXEL_MESSAGE(Error, "{0} can only be used on a Sculpt Volume", this);
		return;
	}

	const TSharedRef<const TVoxelComputeValue<FVoxelSurface>> Compute = GetNodeRuntime().GetCompute(Node.SurfacePin);
	{
		FVoxelScopeLock_Write Lock(Parameter->Data->CriticalSection);
		Parameter->Data->Compute_RequiresLock = MakeVoxelShared<TVoxelComputeValue<FVoxelSurface>>([Compute, Context = GetContext()](const FVoxelQuery& Query)
		{
			return (*Compute)(Query.MakeNewQuery(Context));
		});
	}

	Parameter->Data->Dependency->Invalidate();
}

void FVoxelSetSculptVolumeSourceSurfaceExecNodeRuntime::Destroy()
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedPtr<const FVoxelRuntimeParameter_SculptVolumeStorage> Parameter = FindParameter<FVoxelRuntimeParameter_SculptVolumeStorage>();
	if (!Parameter)
	{
		return;
	}

	{
		FVoxelScopeLock_Write Lock(Parameter->Data->CriticalSection);
		Parameter->Data->Compute_RequiresLock = nullptr;
	}

	Parameter->Data->Dependency->Invalidate();
}