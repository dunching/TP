// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "DragDropActions/VoxelMembersBaseDragDropAction.h"

class UVoxelGraph;
enum class EVoxelGraphParameterType;

BEGIN_VOXEL_NAMESPACE(Graph)

class FLocalVariableDragDropAction : public FVoxelMembersBaseDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FLocalVariableDragDropAction, FGraphSchemaActionDragDropAction)

	static TSharedRef<FLocalVariableDragDropAction> New(const TSharedPtr<FEdGraphSchemaAction>& InAction, const FGuid ParameterGuid, const TWeakObjectPtr<UVoxelGraph> Graph)
	{
		const TSharedRef<FLocalVariableDragDropAction> Operation = MakeVoxelShared<FLocalVariableDragDropAction>();
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

	void SetAltDrag(bool InIsAltDrag) {	bAltDrag = InIsAltDrag; }
	void SetCtrlDrag(bool InIsCtrlDrag) { bControlDrag = InIsCtrlDrag; }

private:
	FGuid ParameterGuid;
	TWeakObjectPtr<UVoxelGraph> WeakGraph;
	bool bControlDrag = false;
	bool bAltDrag = false;

	static void CreateVariable(bool bGetter, const FGuid NewParameterGuid, UEdGraph* Graph, const FVector2D GraphPosition, const EVoxelGraphParameterType ParameterType);
};

END_VOXEL_NAMESPACE(Graph)