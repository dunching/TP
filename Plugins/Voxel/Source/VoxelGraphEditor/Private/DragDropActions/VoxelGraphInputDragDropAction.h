// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "DragDropActions/VoxelMembersBaseDragDropAction.h"

class UVoxelGraph;
struct FVoxelGraphParameter;

BEGIN_VOXEL_NAMESPACE(Graph)

class FInputDragDropAction : public FVoxelMembersBaseDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FInputDragDropAction, FGraphSchemaActionDragDropAction)

	static TSharedRef<FInputDragDropAction> New(const TSharedPtr<FEdGraphSchemaAction>& InAction, const FGuid ParameterGuid, const TWeakObjectPtr<UVoxelGraph> Graph)
	{
		const TSharedRef<FInputDragDropAction> Operation = MakeVoxelShared<FInputDragDropAction>();
		Operation->WeakGraph = Graph;
		Operation->ParameterGuid = ParameterGuid;
		Operation->SourceAction = InAction;
		Operation->Construct();
		return Operation;
	}

	//~ Begin FGraphSchemaActionDragDropAction Interface
	virtual void HoverTargetChanged() override;

	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnPanel(const TSharedRef<SWidget>& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& EdGraph) override;

	virtual void GetDefaultStatusSymbol(const FSlateBrush*& PrimaryBrushOut, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut) const override;
	//~ End FGraphSchemaActionDragDropAction Interface

private:
	FGuid ParameterGuid;
	TWeakObjectPtr<UVoxelGraph> WeakGraph;
};

END_VOXEL_NAMESPACE(Graph)