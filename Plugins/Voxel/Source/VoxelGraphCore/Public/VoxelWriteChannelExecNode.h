// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelChannel.h"
#include "VoxelExecNode.h"
#include "VoxelWriteChannelExecNode.generated.h"

USTRUCT(DisplayName = "Apply To Channel")
struct VOXELGRAPHCORE_API FVoxelWriteChannelExecNode : public FVoxelExecNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelChannelName, Channel, nullptr, VirtualPin);
	VOXEL_INPUT_PIN(int32, Priority, nullptr, VirtualPin);
	VOXEL_INPUT_PIN(FVoxelBox, Bounds, nullptr, VirtualPin);
	VOXEL_INPUT_PIN(FVoxelWildcard, Value, nullptr, VirtualPin);
	// Use this if you're using a SmoothUnion/SmoothIntersection/SmoothSubtraction node and you're seeing glitches
	// Previous brushes will be queried with additional precision - ie, their bounds will be increase by Smoothness
	VOXEL_INPUT_PIN(float, Smoothness, nullptr, VirtualPin, AdvancedDisplay);

	virtual TVoxelUniquePtr<FVoxelExecNodeRuntime> CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const override;

#if WITH_EDITOR
	class FDefinition : public Super::FDefinition
	{
	public:
		GENERATED_VOXEL_NODE_DEFINITION_BODY(FVoxelWriteChannelExecNode);

		virtual bool OnPinDefaultValueChanged(FName PinName, const FVoxelPinValue& NewDefaultValue) override;
	};
#endif
};

class VOXELGRAPHCORE_API FVoxelWriteChannelExecNodeRuntime : public TVoxelExecNodeRuntime<FVoxelWriteChannelExecNode>
{
public:
	using Super::Super;

	virtual void Create() override;
	virtual void Destroy() override;

private:
	DECLARE_VOXEL_PIN_VALUES(
		Channel,
		Priority,
		Bounds,
		Smoothness);

	FVoxelFastCriticalSection CriticalSection;
	TSharedPtr<FVoxelBrushRef> BrushRef;

	void UpdateBrush(const FPinValues& PinValues);
};