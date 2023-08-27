// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphCollapseToMacroSchemaAction.h"

#include "VoxelGraphSchema.h"
#include "VoxelGraphNodeBase.h"
#include "VoxelGraphToolkit.h"
#include "Nodes/VoxelGraphLocalVariableNode.h"
#include "Nodes/VoxelGraphParameterNodeBase.h"

#include "SNodePanel.h"

UEdGraphNode* FVoxelGraphSchemaAction_CollapseToMacro::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	const TSharedPtr<FVoxelGraphToolkit> Toolkit = UVoxelGraphSchema::GetToolkit(ParentGraph);
	if (!ensure(Toolkit))
	{
		return nullptr;
	}

	const TSharedPtr<SGraphEditor> GraphEditor = Toolkit->FindGraphEditor(ParentGraph);
	if (!ensure(GraphEditor))
	{
		return nullptr;
	}

	UVoxelGraph* NewMacroGraph = nullptr;
	{
		FVoxelGraphSchemaAction_NewInlineMacro Action;
		Action.PerformAction(Toolkit->GetActiveEdGraph(), nullptr, Toolkit->FindLocationInGraph());
		NewMacroGraph = Action.NewMacro;
	}

	if (!ensure(NewMacroGraph))
	{
		return nullptr;
	}

	TMap<UEdGraphNode*, TArray<UEdGraphPin*>> NodesToCopy;
	TMap<FGuid, TArray<UVoxelGraphParameterNodeBase*>> Parameters;

	TSet<UObject*> SelectedNodes = GraphEditor->GetSelectedNodes();
	for (UObject* SelectedNode : SelectedNodes)
	{
		if (UVoxelGraphParameterNodeBase* ParameterNode = Cast<UVoxelGraphParameterNodeBase>(SelectedNode))
		{
			Parameters.FindOrAdd(ParameterNode->GetParameter()->Guid, {}).Add(ParameterNode);
		}
		else if (UEdGraphNode* Node = Cast<UEdGraphNode>(SelectedNode))
		{
			TArray<UEdGraphPin*> OuterPins;

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin->HasAnyConnections())
				{
					continue;
				}

				for (const UEdGraphPin* LinkedTo : Pin->LinkedTo)
				{
					if (!ensure(LinkedTo))
					{
						continue;
					}

					const UEdGraphNode* Owner = LinkedTo->GetOwningNode();
					if (!ensure(Owner))
					{
						continue;
					}

					if (!SelectedNodes.Contains(Owner) ||
						Owner->IsA<UVoxelGraphParameterNodeBase>())
					{
						OuterPins.Add(Pin);
						break;
					}
				}
			}

			NodesToCopy.Add(Node, OuterPins);
		}
		else
		{
			ensure(false);
		}
	}

	TMap<FGuid, UEdGraphNode*> NewNodes;
	FVector2D AvgNodePosition;

	// Copy and Paste
	{
		FString ExportedText;
		ExportNodes(NodesToCopy, ExportedText);
		ImportNodes(Toolkit->FindGraphEditor(NewMacroGraph->MainEdGraph), NewMacroGraph->MainEdGraph, ExportedText, NewNodes, AvgNodePosition);
	}

	TMap<UEdGraphPin*, UVoxelGraphParameterNodeBase*> MappedLocalVariables;
	FixupParameterInputsPass(NewMacroGraph, Parameters, MappedLocalVariables);
	FixupLocalVariableDeclarationsPass(NewMacroGraph, Parameters, NewNodes);

	for (const auto& It : NodesToCopy)
	{
		UEdGraphNode* NewNode = NewNodes.FindRef(It.Key->NodeGuid);
		if (!ensure(NewNode))
		{
			continue;
		}

		for (UEdGraphPin* Pin : It.Value)
		{
			UEdGraphPin* NewNodePin = NewNode->FindPin(Pin->PinName, Pin->Direction);
			if (!ensure(NewNodePin))
			{
				continue;
			}

			if (Pin->Direction == EGPD_Input)
			{
				if (NewNodePin->LinkedTo.Num() != 0)
				{
					continue;
				}

				FVector2D Position;
				Position.X = NewNodePin->Direction == EGPD_Input ? NewNode->NodePosX - 200 : NewNode->NodePosX + 400;
				Position.Y = NewNode->NodePosY + 75;

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!ensure(LinkedPin))
					{
						continue;
					}

					UEdGraphNode* OwningNode = LinkedPin->GetOwningNode();
					if (!ensure(OwningNode))
					{
						continue;
					}

					Position.X = OwningNode->NodePosX - AvgNodePosition.X;
					Position.Y = OwningNode->NodePosY - AvgNodePosition.Y;
				}

				if (UVoxelGraphParameterNodeBase* LocalVariableNode = MappedLocalVariables.FindRef(Pin))
				{
					FVoxelGraphSchemaAction_NewParameterUsage Action;
					Action.bDeclaration = false;
					Action.Guid = LocalVariableNode->Guid;
					Action.ParameterType = EVoxelGraphParameterType::LocalVariable;
					Action.PinType = LocalVariableNode->GetParameter()->Type;
					Action.PerformAction(NewNode->GetGraph(), NewNodePin, Position, false);
					continue;
				}

				FVoxelGraphSchemaAction_NewParameter Action;
				Action.ParameterType = EVoxelGraphParameterType::Input;
				Action.PerformAction(NewNode->GetGraph(), NewNodePin, Position, false);
				continue;
			}

			FVector2D Position;
			Position.X = NewNode->NodePosX + 400;
			Position.Y = NewNode->NodePosY + 75;

			FVoxelGraphSchemaAction_NewParameter Action;
			Action.ParameterType = EVoxelGraphParameterType::Output;
			Action.PerformAction(NewNode->GetGraph(), NewNodePin, Position, false);
		}
	}

	return nullptr;
}

void FVoxelGraphSchemaAction_CollapseToMacro::FixupParameterInputsPass(const UVoxelGraph* Graph, const TMap<FGuid, TArray<UVoxelGraphParameterNodeBase*>>& Parameters, TMap<UEdGraphPin*, UVoxelGraphParameterNodeBase*>& OutMappedLocalVariables) const
{
	FVector2D DeclarationsPos = FVector2D::ZeroVector;
	for (const UEdGraphNode* Node : Graph->MainEdGraph->Nodes)
	{
		DeclarationsPos.X = FMath::Min(Node->NodePosX, DeclarationsPos.X);
		DeclarationsPos.Y = FMath::Min(Node->NodePosY, DeclarationsPos.Y);
	}

	DeclarationsPos.X -= 500.f;

	for (const auto& It : Parameters)
	{
		if (It.Value.Num() == 1)
		{
			continue;
		}

		bool bSkip = false;
		for (const UVoxelGraphParameterNodeBase* ParameterNode : It.Value)
		{
			if (ParameterNode->GetParameter()->ParameterType == EVoxelGraphParameterType::Output)
			{
				bSkip = true;
				break;
			}

			if (ParameterNode->IsA<UVoxelGraphLocalVariableDeclarationNode>())
			{
				bSkip = true;
				break;
			}
		}

		if (bSkip)
		{
			continue;
		}

		UVoxelGraphNodeBase* MacroInput;
		{
			FVoxelGraphSchemaAction_NewParameter Action;
			Action.ParameterType = EVoxelGraphParameterType::Input;

			FVector2D Position;
			Position.X = DeclarationsPos.X;
			Position.Y = DeclarationsPos.Y;

			MacroInput = Cast<UVoxelGraphNodeBase>(Action.PerformAction(Graph->MainEdGraph, It.Value[0]->GetOutputPin(0), Position, true));
		}

		if (!ensure(MacroInput))
		{
			continue;
		}

		UVoxelGraphParameterNodeBase* LocalVariableDeclaration;
		{
			FVoxelGraphSchemaAction_NewParameter Action;
			Action.ParameterType = EVoxelGraphParameterType::LocalVariable;

			FVector2D Position;
			Position.X = DeclarationsPos.X + 200.f;
			Position.Y = DeclarationsPos.Y;

			LocalVariableDeclaration = Cast<UVoxelGraphParameterNodeBase>(Action.PerformAction(Graph->MainEdGraph, MacroInput->GetOutputPin(0), Position, true));
		}

		if (!ensure(LocalVariableDeclaration))
		{
			MacroInput->DestroyNode();
			continue;
		}

		DeclarationsPos.Y += 200.f;

		for (const UVoxelGraphParameterNodeBase* ParameterNode : It.Value)
		{
			if (ParameterNode->IsA<UVoxelGraphLocalVariableDeclarationNode>())
			{
				continue;
			}

			UEdGraphPin* OutputPin = ParameterNode->GetOutputPin(0);
			if (!ensure(OutputPin))
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
			{
				OutMappedLocalVariables.Add(LinkedPin, LocalVariableDeclaration);
			}
		}
	}
}

void FVoxelGraphSchemaAction_CollapseToMacro::FixupLocalVariableDeclarationsPass(const UVoxelGraph* Graph, const TMap<FGuid, TArray<UVoxelGraphParameterNodeBase*>>& Parameters, const TMap<FGuid, UEdGraphNode*>& NewNodes)
{
	for (const auto& It : Parameters)
	{
		const UVoxelGraphParameterNodeBase* LocalVariableNode = nullptr;
		for (const UVoxelGraphParameterNodeBase* ParameterNode : It.Value)
		{
			if (ParameterNode->IsA<UVoxelGraphLocalVariableDeclarationNode>())
			{
				LocalVariableNode = ParameterNode;
				break;
			}
		}

		if (!LocalVariableNode)
		{
			continue;
		}

		UEdGraphPin* InputPin = nullptr;

		{
			const UEdGraphPin* Pin = LocalVariableNode->GetInputPin(0);
			if (!ensure(Pin))
			{
				continue;
			}

			if (Pin->LinkedTo.Num() == 0)
			{
				// TODO: What to do if 0?
				continue;
			}

			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!ensure(LinkedPin))
				{
					continue;
				}

				const UEdGraphNode* OwningNode = LinkedPin->GetOwningNode();
				if (!ensure(OwningNode))
				{
					continue;
				}

				if (const UEdGraphNode* NewNode = NewNodes.FindRef(OwningNode->NodeGuid))
				{
					if (UEdGraphPin* NewPin = NewNode->FindPin(LinkedPin->PinName, EGPD_Output))
					{
						InputPin = NewPin;
					}
					break;
				}
			}

			if (!InputPin)
			{
				// TODO: Connected to outer node...
				continue;
			}
		}

		if (!InputPin)
		{
			continue; // TODO: Figure this out
		}

		{
			const UEdGraphPin* Pin = LocalVariableNode->GetOutputPin(0);
			if (!ensure(Pin))
			{
				continue;
			}

			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!ensure(LinkedPin))
				{
					continue;
				}

				const UEdGraphNode* OwningNode = LinkedPin->GetOwningNode();
				if (!ensure(OwningNode))
				{
					continue;
				}

				if (const UEdGraphNode* NewNode = NewNodes.FindRef(OwningNode->NodeGuid))
				{
					if (UEdGraphPin* NewPin = NewNode->FindPin(LinkedPin->PinName, EGPD_Input))
					{
						InputPin->MakeLinkTo(NewPin);
					}
				}
			}
		}

		if (It.Value.Num() > 1)
		{

		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGraphSchemaAction_CollapseToMacro::ExportNodes(const TMap<UEdGraphNode*, TArray<UEdGraphPin*>>& Nodes, FString& ExportText) const
{
	TSet<UEdGraphNode*> NodesToCopy;
	for (auto& It : Nodes)
	{
		if (!It.Key->CanDuplicateNode())
		{
			continue;
		}

		It.Key->PrepareForCopying();
		NodesToCopy.Add(It.Key);
	}

	FEdGraphUtilities::ExportNodesToText(reinterpret_cast<TSet<UObject*>&>(NodesToCopy), ExportText);
}

void FVoxelGraphSchemaAction_CollapseToMacro::ImportNodes(const TSharedPtr<SGraphEditor>& GraphEditor, UEdGraph* EdGraph, const FString& ExportText, TMap<FGuid, UEdGraphNode*>& NewNodes, FVector2D& OutAvgNodePosition) const
{
	const FVoxelTransaction Transaction(EdGraph, "Paste nodes");
	FVoxelGraphDelayOnGraphChangedScope OnGraphChangedScope;

	// Clear the selection set (newly pasted stuff will be selected)
	GraphEditor->ClearSelectionSet();

	TSet<UEdGraphNode*> PastedNodes;
	// Import the nodes
	FEdGraphUtilities::ImportNodesFromText(EdGraph, ExportText, PastedNodes);

	TSet<UEdGraphNode*> CopyPastedNodes = PastedNodes;
	for (UEdGraphNode* Node : CopyPastedNodes)
	{
		if (UVoxelGraphNodeBase* VoxelNode = Cast<UVoxelGraphNodeBase>(Node))
		{
			if (!VoxelNode->CanPasteVoxelNode(PastedNodes))
			{
				Node->DestroyNode();
				PastedNodes.Remove(Node);
			}
		}
	}

	// Average position of nodes so we can move them while still maintaining relative distances to each other
	OutAvgNodePosition = FVector2D::ZeroVector;

	for (const UEdGraphNode* Node : PastedNodes)
	{
		OutAvgNodePosition.X += Node->NodePosX;
		OutAvgNodePosition.Y += Node->NodePosY;
	}

	if (PastedNodes.Num() > 0)
	{
		OutAvgNodePosition.X /= PastedNodes.Num();
		OutAvgNodePosition.Y /= PastedNodes.Num();
	}

	for (UEdGraphNode* Node : PastedNodes)
	{
		// Select the newly pasted stuff
		GraphEditor->SetNodeSelection(Node, true);

		Node->NodePosX = (Node->NodePosX - OutAvgNodePosition.X);
		Node->NodePosY = (Node->NodePosY - OutAvgNodePosition.Y);

		Node->SnapToGrid(SNodePanel::GetSnapGridSize());

		NewNodes.Add(Node->NodeGuid, Node);

		// Give new node a different Guid from the old one
		Node->CreateNewGuid();
	}

	// Post paste for local variables
	for (UEdGraphNode* Node : PastedNodes)
	{
		if (UVoxelGraphNodeBase* VoxelNode = Cast<UVoxelGraphNodeBase>(Node))
		{
			VoxelNode->PostPasteVoxelNode(PastedNodes);
		}
	}

	// Update UI
	GraphEditor->NotifyGraphChanged();
}