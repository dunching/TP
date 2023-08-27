// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelSurface.h"
#include "VoxelExecNode.h"
#include "VoxelSetSculptVolumeSurfaceExecNode.generated.h"

USTRUCT(DisplayName = "Set Sculpt Volume Surface")
struct VOXELGRAPHCORE_API FVoxelSetSculptVolumeSurfaceExecNode : public FVoxelExecNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelSurface, Surface, nullptr, VirtualPin);
	VOXEL_INPUT_PIN(FVoxelBox, Bounds, nullptr, VirtualPin);

	virtual TVoxelUniquePtr<FVoxelExecNodeRuntime> CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const override;
};

class VOXELGRAPHCORE_API FVoxelSetSculptVolumeSurfaceExecNodeRuntime : public TVoxelExecNodeRuntime<FVoxelSetSculptVolumeSurfaceExecNode>
{
public:
	using Super::Super;

	//~ Begin FVoxelExecNodeRuntime Interface
	virtual void Create() override;
	virtual void Destroy() override;
	//~ End FVoxelExecNodeRuntime Interface
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelRuntimeParameter_SetSculptVolumeSurface : public FVoxelRuntimeParameter
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	mutable FVoxelFastCriticalSection CriticalSection;
	mutable TWeakPtr<FVoxelSetSculptVolumeSurfaceExecNodeRuntime> WeakRuntime;
};