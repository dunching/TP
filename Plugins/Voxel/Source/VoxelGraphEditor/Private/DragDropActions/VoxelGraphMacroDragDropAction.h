// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "DragDropActions/VoxelMembersBaseDragDropAction.h"

struct FVoxelGraphToolkit;
class UVoxelGraph;

BEGIN_VOXEL_NAMESPACE(Graph)

class FMacroDragDropAction : public FVoxelMembersBaseDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FMacroDragDropAction, FGraphSchemaActionDragDropAction)

	static TSharedRef<FMacroDragDropAction> New(const TSharedPtr<FEdGraphSchemaAction>& InAction, const TWeakObjectPtr<UVoxelGraph> Macro)
	{
		const TSharedRef<FMacroDragDropAction> Operation = MakeVoxelShared<FMacroDragDropAction>();
		Operation->WeakMacroGraph = Macro;
		Operation->SourceAction = InAction;
		Operation->Construct();
		return Operation;
	}

	//~ Begin FGraphSchemaActionDragDropAction Interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnPanel(const TSharedRef<SWidget>& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& EdGraph) override;
	//~ End FGraphSchemaActionDragDropAction Interface

private:
	TWeakObjectPtr<UVoxelGraph> WeakMacroGraph;
};

END_VOXEL_NAMESPACE(Graph)