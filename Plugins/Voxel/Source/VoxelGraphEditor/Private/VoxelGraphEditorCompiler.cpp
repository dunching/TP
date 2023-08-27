// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphEditorCompiler.h"
#include "VoxelGraphCompilerUtilities.h"
#include "VoxelGraphEditorCompilerPasses.h"
#include "VoxelFunctionNode.h"
#include "VoxelGraphMessages.h"
#include "Nodes/VoxelGraphKnotNode.h"
#include "Nodes/VoxelGraphMacroNode.h"
#include "Nodes/VoxelGraphStructNode.h"
#include "Nodes/VoxelGraphParameterNode.h"
#include "Nodes/VoxelGraphMacroParameterNode.h"

VOXELGRAPHEDITOR_API bool GIsVoxelGraphCompiling = false;

VOXEL_RUN_ON_STARTUP_EDITOR(RegisterGraphTranslator)
{
	VOXEL_USE_NAMESPACE(Graph);

	FCompilerUtilities::ForceTranslateGraph_EditorOnly = [](UVoxelGraph& Graph)
	{
		VOXEL_SCOPE_COUNTER("ForceTranslateGraph");

		ensure(!GIsVoxelGraphCompiling);
		GIsVoxelGraphCompiling = true;
		ON_SCOPE_EXIT
		{
			ensure(GIsVoxelGraphCompiling);
			GIsVoxelGraphCompiling = false;
		};

		if (!Graph.MainEdGraph)
		{
			// New asset
			return;
		}

		for (UEdGraphNode* Node : Graph.MainEdGraph->Nodes)
		{
			Node->ReconstructNode();
		}

		FEditorCompilerUtilities::CompileGraph(Graph);
	};

	FCompilerUtilities::RaiseOrphanWarnings_EditorOnly = [](UVoxelGraph& Graph)
	{
		VOXEL_SCOPE_COUNTER("RaiseOrphanWarnings");

		if (!ensure(Graph.MainEdGraph))
		{
			return;
		}

		for (const UEdGraphNode* Node : Graph.MainEdGraph->Nodes)
		{
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->bOrphanedPin)
				{
					VOXEL_MESSAGE(Warning, "Orphaned pin on {0}: {1}", Node, Pin->GetDisplayName());
				}
			}
		}
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BEGIN_VOXEL_NAMESPACE(Graph)

void FEditorPin::Check(const FEditorGraph& Graph) const
{
	ensure(!PinName.IsNone());
	ensure(Graph.Nodes.Contains(Node));
	ensure(Node->InputPins.Contains(this) || Node->OutputPins.Contains(this));

	ensure(!LinkedTo.Contains(this));
	for (const FEditorPin* Pin : LinkedTo)
	{
		ensure(Graph.Nodes.Contains(Pin->Node));
		ensure(Pin->Node->InputPins.Contains(Pin) || Pin->Node->OutputPins.Contains(Pin));
		ensure(Pin->LinkedTo.Contains(this));
	}
}

FName FEditorNode::GetNodeId() const
{
	if (const UVoxelGraphMacroParameterNode* MacroParameterNode = Cast<UVoxelGraphMacroParameterNode>(GraphNode))
	{
		if (MacroParameterNode->Type == EVoxelGraphParameterType::Output)
		{
			// Use Output.Guid
			return FName("Output." + MacroParameterNode->Guid.ToString());
		}
	}

	if (const UVoxelGraphStructNode* StructNode = Cast<UVoxelGraphStructNode>(GraphNode))
	{
		if (ensureVoxelSlow(StructNode->Struct.IsValid()))
		{
			if (const FVoxelFunctionNode* FunctionNode = Cast<FVoxelFunctionNode>(*StructNode->Struct))
			{
				if (const UFunction* Function = FunctionNode->GetFunction())
				{
					// Use Class.Function.NodeId
					return FName(Function->GetOuterUClass()->GetName() + "." + Function->GetName() + "." + GraphNode->GetName());
				}
			}

			// Use Struct.NodeId
			return FName(StructNode->Struct->GetStruct()->GetName() + "." + GraphNode->GetName());
		}
	}

	return GraphNode->GetFName();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FEditorCompilerUtilities::CompileGraph(UVoxelGraph& Graph)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(Graph.MainEdGraph))
	{
		return false;
	}

	FVoxelGraphMessages Messages(Graph.MainEdGraph);

	// Clear to not have outdated errors
	Graph.CompiledGraph.bIsValid = false;
	Graph.CompiledGraph.Nodes = {};

	for (const UEdGraphNode* Node : Graph.MainEdGraph->Nodes)
	{
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->bOrphanedPin)
			{
				VOXEL_MESSAGE(Warning, "Orphaned pin on {0}: {1}", Node, Pin->GetDisplayName());
			}

			if (Pin->ParentPin)
			{
				ensure(Pin->ParentPin->SubPins.Contains(Pin));
			}

			for (const UEdGraphPin* SubPin : Pin->SubPins)
			{
				ensure(SubPin->ParentPin == Pin);
			}
		}
	}

	TArray<FFEditorCompilerPass> Passes;
	Passes.Add(FEditorCompilerPasses::RemoveDisabledNodes);
	Passes.Add(FEditorCompilerPasses::SetupPreview);
	Passes.Add(FEditorCompilerPasses::AddToBufferNodes);
	Passes.Add(FEditorCompilerPasses::RemoveSplitPins);
	Passes.Add(FEditorCompilerPasses::FixupLocalVariables);

	FEditorGraph EditorGraph;
	if (!CompileGraph(*Graph.MainEdGraph, EditorGraph, Passes))
	{
		VOXEL_MESSAGE(Error, "Failed to translate graph");
		return false;
	}

	FVoxelCompiledGraph CompiledGraph;

	for (const FEditorNode* Node : EditorGraph.Nodes)
	{
		FVoxelCompiledNode CompiledNode;
		CompiledNode.Ref.Graph = &Graph;
		CompiledNode.Ref.NodeId = Node->GetNodeId();

		if (ensure(Node->SourceGraphNode))
		{
			CompiledNode.Ref.DebugName = *Node->SourceGraphNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			CompiledNode.Ref.EdGraphNodeName_EditorOnly = Node->SourceGraphNode->GetFName();
		}

		if (const UVoxelGraphStructNode* StructNode = Cast<UVoxelGraphStructNode>(Node->GraphNode))
		{
			CompiledNode.Type = EVoxelCompiledNodeType::Struct;
			CompiledNode.Struct = StructNode->Struct;
		}
		else if (const UVoxelGraphMacroNode* MacroNode = Cast<UVoxelGraphMacroNode>(Node->GraphNode))
		{
			CompiledNode.Type = EVoxelCompiledNodeType::Macro;
			CompiledNode.Graph = MacroNode->GraphInterface;
		}
		else if (const UVoxelGraphParameterNode* ParameterNode = Cast<UVoxelGraphParameterNode>(Node->GraphNode))
		{
			CompiledNode.Type = EVoxelCompiledNodeType::Parameter;
			CompiledNode.Guid = ParameterNode->Guid;
		}
		else if (const UVoxelGraphMacroParameterNode* MacroParameterNode = Cast<UVoxelGraphMacroParameterNode>(Node->GraphNode))
		{
			switch (MacroParameterNode->Type)
			{
			default:
			{
				ensure(false);
				return false;
			}
			case EVoxelGraphParameterType::Input:
			{
				CompiledNode.Type = EVoxelCompiledNodeType::Input;
				CompiledNode.Guid = MacroParameterNode->Guid;
			}
			break;
			case EVoxelGraphParameterType::Output:
			{
				CompiledNode.Type = EVoxelCompiledNodeType::Output;
				CompiledNode.Guid = MacroParameterNode->Guid;
			}
			break;
			}
		}
		else
		{
			ensure(false);
			return false;
		}

		if (!ensure(!CompiledGraph.Nodes.Contains(CompiledNode.Ref.NodeId)))
		{
			return false;
		}

		CompiledGraph.Nodes.Add(CompiledNode.Ref.NodeId, CompiledNode);
	}

	TMap<const FEditorPin*, FVoxelCompiledPinRef> PinMap;
	for (const FEditorNode* Node : EditorGraph.Nodes)
	{
		FVoxelCompiledNode& CompiledNode = CompiledGraph.Nodes[Node->GetNodeId()];

		const auto MakePin = [&](const FEditorPin& Pin)
		{
			FVoxelCompiledPin CompiledPin;
			CompiledPin.Type = Pin.PinType;
			CompiledPin.PinName = Pin.PinName;
			CompiledPin.DefaultValue = Pin.DefaultValue;
			return CompiledPin;
		};

		for (const FEditorPin* Pin : Node->InputPins)
		{
			if (!ensure(!CompiledNode.InputPins.Contains(Pin->PinName)))
			{
				return false;
			}
			CompiledNode.InputPins.Add(Pin->PinName, MakePin(*Pin));

			PinMap.Add(Pin, FVoxelCompiledPinRef{ CompiledNode.Ref.NodeId, Pin->PinName, true });
		}
		for (const FEditorPin* Pin : Node->OutputPins)
		{
			if (!ensure(!CompiledNode.OutputPins.Contains(Pin->PinName)))
			{
				return false;
			}
			CompiledNode.OutputPins.Add(Pin->PinName, MakePin(*Pin));

			PinMap.Add(Pin, FVoxelCompiledPinRef{ CompiledNode.Ref.NodeId, Pin->PinName, false });
		}
	}

	// Link the pins once they're all allocated
	for (const FEditorNode* Node : EditorGraph.Nodes)
	{
		const auto AddPin = [&](const FEditorPin* Pin)
		{
			FVoxelCompiledPin* CompiledPin = CompiledGraph.FindPin(PinMap[Pin]);
			if (!ensure(CompiledPin))
			{
				return false;
			}

			for (const FEditorPin* OtherPin : Pin->LinkedTo)
			{
				CompiledPin->LinkedTo.Add(PinMap[OtherPin]);
			}

			return true;
		};

		for (const FEditorPin* Pin : Node->InputPins)
		{
			if (!ensure(AddPin(Pin)))
			{
				return false;
			}
		}
		for (const FEditorPin* Pin : Node->OutputPins)
		{
			if (!ensure(AddPin(Pin)))
			{
				return false;
			}
		}
	}

	Graph.CompiledGraph.bIsValid = true;
	Graph.CompiledGraph.Nodes = CompiledGraph.Nodes;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FEditorCompilerUtilities::CompileGraph(
	const UEdGraph& EdGraph,
	FEditorGraph& OutGraph,
	const TArray<FFEditorCompilerPass>& Passes)
{
	VOXEL_FUNCTION_COUNTER();

	TMap<UObject*, UObject*> CreatedObjects;

	FObjectDuplicationParameters DuplicationParameters(ConstCast(&EdGraph), EdGraph.GetOuter());
	DuplicationParameters.CreatedObjects = &CreatedObjects;
	DuplicationParameters.ApplyFlags |= RF_Transient;
	DuplicationParameters.FlagMask &= ~RF_Transactional;

	FEditorCompiler Compiler;
	Compiler.Graph = CastChecked<UEdGraph>(StaticDuplicateObjectEx(DuplicationParameters));
	ON_SCOPE_EXIT
	{
		Compiler.Graph->MarkAsGarbage();
	};

	for (auto& It : CreatedObjects)
	{
		UEdGraphNode* Source = Cast<UEdGraphNode>(It.Key);
		UEdGraphNode* Duplicate = Cast<UEdGraphNode>(It.Value);
		if (!Source || !Duplicate)
		{
			continue;
		}

		Compiler.NodeSourceMap.Add(Duplicate, Source);
	}

	// Remove orphaned pins from duplicated graph
	for (UEdGraphNode* Node : Compiler.Graph->Nodes)
	{
		for (UEdGraphPin* Pin : MakeCopy(Node->Pins))
		{
			if (Pin->bOrphanedPin)
			{
				Node->RemovePin(Pin);
			}
		}
	}

	CheckGraph(Compiler);

	if (GVoxelGraphMessages->HasError())
	{
		return false;
	}

	for (const FFEditorCompilerPass& Pass : Passes)
	{
		if (!Pass(Compiler))
		{
			return false;
		}

		CheckGraph(Compiler);

		if (GVoxelGraphMessages->HasError())
		{
			return false;
		}
	}

	return TranslateGraph(Compiler, OutGraph);
}

bool FEditorCompilerUtilities::TranslateGraph(
	const FEditorCompiler& Compiler,
	FEditorGraph& OutGraph)
{
	TMap<UEdGraphPin*, FEditorPin*> AllPins;
	TMap<UEdGraphNode*, FEditorNode*> AllNodes;

	const auto AllocPin = [&](UEdGraphPin* GraphPin)
	{
		FEditorPin*& Pin = AllPins.FindOrAdd(GraphPin);
		if (!Pin)
		{
			Pin = OutGraph.NewPin();
			Pin->Node = AllNodes[GraphPin->GetOwningNode()];
			Pin->PinName = GraphPin->PinName;
			Pin->PinType = GraphPin->PinType;

			if (Pin->PinType.HasPinDefaultValue())
			{
				Pin->DefaultValue = FVoxelPinValue::MakeFromPinDefaultValue(*GraphPin);
			}
			else
			{
				ensure(!GraphPin->DefaultObject);
				ensure(GraphPin->DefaultValue.IsEmpty());
				ensure(GraphPin->AutogeneratedDefaultValue.IsEmpty());
			}
		}
		return Pin;
	};
	const auto AllocNode = [&](UVoxelGraphNode* GraphNode)
	{
		FEditorNode*& Node = AllNodes.FindOrAdd(GraphNode);
		if (!Node)
		{
			Node = OutGraph.NewNode();
			Node->GraphNode = GraphNode;
			Node->SourceGraphNode = Compiler.GetSourceNode(GraphNode);

			for (UEdGraphPin* Pin : GraphNode->GetInputPins())
			{
				Node->InputPins.Add(AllocPin(Pin));
			}
			for (UEdGraphPin* Pin : GraphNode->GetOutputPins())
			{
				Node->OutputPins.Add(AllocPin(Pin));
			}
		}
		return Node;
	};

	for (UEdGraphNode* GraphNode : Compiler.GetNodesCopy())
	{
		UVoxelGraphNode* Node = Cast<UVoxelGraphNode>(GraphNode);
		if (!Node ||
			Node->IsA<UVoxelGraphKnotNode>())
		{
			continue;
		}

		OutGraph.Nodes.Add(AllocNode(Node));
	}

	for (const auto& It : AllPins)
	{
		UEdGraphPin* GraphPin = It.Key;
		FEditorPin* Pin = It.Value;

		ensure(!GraphPin->ParentPin);
		ensure(GraphPin->SubPins.Num() == 0);
		ensure(!GraphPin->bOrphanedPin);

		for (const UEdGraphPin* LinkedTo : GraphPin->LinkedTo)
		{
			if (UVoxelGraphKnotNode* Knot = Cast<UVoxelGraphKnotNode>(LinkedTo->GetOwningNode()))
			{
				for (const UEdGraphPin* LinkedToKnot : Knot->GetLinkedPins(GraphPin->Direction))
				{
					if (ensure(AllPins.Contains(LinkedToKnot)))
					{
						Pin->LinkedTo.Add(AllPins[LinkedToKnot]);
					}
				}
				continue;
			}

			if (ensure(AllPins.Contains(LinkedTo)))
			{
				Pin->LinkedTo.Add(AllPins[LinkedTo]);
			}
		}
	}

	for (const FEditorNode* Node : OutGraph.Nodes)
	{
		check(Node->GraphNode);

		for (const FEditorPin* Pin : Node->InputPins)
		{
			check(Pin->Node == Node);
			Pin->Check(OutGraph);
		}
		for (const FEditorPin* Pin : Node->OutputPins)
		{
			check(Pin->Node == Node);
			Pin->Check(OutGraph);
		}
	}

	return true;
}

void FEditorCompilerUtilities::CheckGraph(const FEditorCompiler& Compiler)
{
	for (const UEdGraphNode* Node : Compiler.GetNodesCopy())
	{
		if (!ensure(Node))
		{
			VOXEL_MESSAGE(Error, "{0} is invalid", Node);
			return;
		}

		ensure(Node->HasAllFlags(RF_Transient));
		ensure(Compiler.GetSourceNode(Node));

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!ensure(Pin))
			{
				VOXEL_MESSAGE(Error, "{0} is invalid", Pin);
				return;
			}

			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!ensure(LinkedPin))
				{
					VOXEL_MESSAGE(Error, "{0} is invalid", LinkedPin);
					return;
				}

				if (!ensure(LinkedPin->LinkedTo.Contains(Pin)))
				{
					VOXEL_MESSAGE(Error, "Link from {0} to {1} is invalid", Pin, LinkedPin);
					return;
				}
			}
		}
	}
}

END_VOXEL_NAMESPACE(Graph)