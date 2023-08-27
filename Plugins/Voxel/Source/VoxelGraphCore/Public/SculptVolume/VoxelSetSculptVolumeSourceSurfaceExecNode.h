// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelSurface.h"
#include "VoxelExecNode.h"
#include "VoxelSetSculptVolumeSourceSurfaceExecNode.generated.h"

USTRUCT(DisplayName = "Set Sculpt Volume Source Surface")
struct VOXELGRAPHCORE_API FVoxelSetSculptVolumeSourceSurfaceExecNode : public FVoxelExecNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelSurface, Surface, nullptr, VirtualPin);

	virtual TVoxelUniquePtr<FVoxelExecNodeRuntime> CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const override;
};

class VOXELGRAPHCORE_API FVoxelSetSculptVolumeSourceSurfaceExecNodeRuntime : public TVoxelExecNodeRuntime<FVoxelSetSculptVolumeSourceSurfaceExecNode>
{
public:
	using Super::Super;

	//~ Begin FVoxelExecNodeRuntime Interface
	virtual void Create() override;
	virtual void Destroy() override;
	//~ End FVoxelExecNodeRuntime Interface
};