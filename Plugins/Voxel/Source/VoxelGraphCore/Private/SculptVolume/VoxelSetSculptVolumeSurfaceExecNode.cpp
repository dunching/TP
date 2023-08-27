// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SculptVolume/VoxelSetSculptVolumeSurfaceExecNode.h"

TVoxelUniquePtr<FVoxelExecNodeRuntime> FVoxelSetSculptVolumeSurfaceExecNode::CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const
{
	return MakeVoxelUnique<FVoxelSetSculptVolumeSurfaceExecNodeRuntime>(SharedThis);
}

void FVoxelSetSculptVolumeSurfaceExecNodeRuntime::Create()
{
	Super::Create();

	const TSharedPtr<const FVoxelRuntimeParameter_SetSculptVolumeSurface> Parameter = FindParameter<FVoxelRuntimeParameter_SetSculptVolumeSurface>();
	if (!Parameter)
	{
		return;
	}

	VOXEL_SCOPE_LOCK(Parameter->CriticalSection);

	if (const TSharedPtr<FVoxelSetSculptVolumeSurfaceExecNodeRuntime> Runtime = Parameter->WeakRuntime.Pin())
	{
		VOXEL_MESSAGE(Error, "Multiple Set Sculpt Volume Surface nodes: {0}, {1}", this, Runtime.Get());
		return;
	}

	Parameter->WeakRuntime = SharedThis(this);
}

void FVoxelSetSculptVolumeSurfaceExecNodeRuntime::Destroy()
{
	Super::Destroy();

	const TSharedPtr<const FVoxelRuntimeParameter_SetSculptVolumeSurface> Parameter = FindParameter<FVoxelRuntimeParameter_SetSculptVolumeSurface>();
	if (!Parameter)
	{
		return;
	}

	VOXEL_SCOPE_LOCK(Parameter->CriticalSection);

	if (GetWeakPtrObject_Unsafe(Parameter->WeakRuntime) == this)
	{
		Parameter->WeakRuntime = {};
	}
}