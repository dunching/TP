// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "SVoxelGraphPinParameter.h"
#include "SVoxelBlueprintGraphNode.h"
#include "SVoxelGraphPinChannelName_K2.h"
#include "K2Node_QueryVoxelChannel.h"
#include "K2Node_VoxelGraphParameterBase.h"

VOXEL_DEFAULT_MODULE(VoxelBlueprintEditor);

class FVoxelBlueprintGraphPinFactory : public FGraphPanelPinFactory
{
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* Pin) const override
	{
		VOXEL_FUNCTION_COUNTER();

		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name &&
			Pin->PinName == UK2Node_VoxelGraphParameterBase::ParameterPinName)
		{
			const UObject* Outer = Pin->GetOuter();

			if (Outer->IsA<UK2Node_VoxelGraphParameterBase>())
			{
				return SNew(SVoxelGraphPinParameter, Pin);
			}
		}

		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name &&
			Pin->PinName == STATIC_FNAME("Channel"))
		{
			const UObject* Outer = Pin->GetOuter();

			if (Outer->IsA<UK2Node_QueryVoxelChannelBase>())
			{
				return SNew(SVoxelGraphPinChannelName_K2, Pin);
			}
		}

		return nullptr;
	}
};

class FVoxelBlueprintGraphNodeFactory : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* InNode) const override
	{
		VOXEL_FUNCTION_COUNTER();

		if (UK2Node_VoxelBaseNode* Node = Cast<UK2Node_VoxelBaseNode>(InNode))
		{
			return SNew(SVoxelBlueprintGraphNode, Node);
		}

		return nullptr;
	}
};

VOXEL_RUN_ON_STARTUP_EDITOR(RegisterGraphConnectionDrawingPolicyFactory)
{
	FEdGraphUtilities::RegisterVisualNodeFactory(MakeVoxelShared<FVoxelBlueprintGraphNodeFactory>());
	FEdGraphUtilities::RegisterVisualPinFactory(MakeVoxelShared<FVoxelBlueprintGraphPinFactory>());
}