// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelGraphSchemaAction.h"
#include "VoxelGraphCollapseToMacroSchemaAction.generated.h"

class UVoxelGraph;
class SGraphEditor;
class UVoxelGraphParameterNodeBase;

USTRUCT()
struct FVoxelGraphSchemaAction_CollapseToMacro : public FVoxelGraphSchemaAction
{
	GENERATED_BODY();

public:
	using FVoxelGraphSchemaAction::FVoxelGraphSchemaAction;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

private:
	void FixupParameterInputsPass(const UVoxelGraph* Graph, const TMap<FGuid, TArray<UVoxelGraphParameterNodeBase*>>& Parameters, TMap<UEdGraphPin*, UVoxelGraphParameterNodeBase*>& OutMappedLocalVariables) const;
	void FixupLocalVariableDeclarationsPass(const UVoxelGraph* Graph, const TMap<FGuid, TArray<UVoxelGraphParameterNodeBase*>>& Parameters, const TMap<FGuid, UEdGraphNode*>& NewNodes);

private:
	void ExportNodes(const TMap<UEdGraphNode*, TArray<UEdGraphPin*>>& Nodes, FString& ExportText) const;
	void ImportNodes(const TSharedPtr<SGraphEditor>& GraphEditor, UEdGraph* EdGraph, const FString& ExportText, TMap<FGuid, UEdGraphNode*>& NewNodes, FVector2D& OutAvgNodePosition) const;
};