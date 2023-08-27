// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphEditorCompilerPasses.h"
#include "Preview/VoxelPreviewNode.h"
#include "VoxelNodeLibrary.h"
#include "VoxelFunctionNode.h"
#include "Nodes/VoxelGraphMacroNode.h"
#include "Nodes/VoxelGraphStructNode.h"
#include "Nodes/VoxelGraphLocalVariableNode.h"
#include "FunctionLibrary/VoxelBasicFunctionLibrary.h"

BEGIN_VOXEL_NAMESPACE(Graph)

bool FEditorCompilerPasses::RemoveDisabledNodes(FEditorCompiler& Compiler)
{
	for (UEdGraphNode* Node : Compiler.GetNodesCopy())
	{
		if (Node->IsNodeEnabled())
		{
			continue;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			ensure(Pin->Direction == EGPD_Input);
		}

		Node->DestroyNode();
	}

	return true;
}

bool FEditorCompilerPasses::SetupPreview(FEditorCompiler& Compiler)
{
	UVoxelGraphNodeBase* NodeToPreview = nullptr;
	for (UEdGraphNode* Node : Compiler.GetNodesCopy())
	{
		UVoxelGraphNodeBase* VoxelNode = Cast<UVoxelGraphNodeBase>(Node);
		if (!VoxelNode ||
			!VoxelNode->bEnablePreview)
		{
			continue;
		}

		ensure(!NodeToPreview);
		NodeToPreview = VoxelNode;
	}

	if (!NodeToPreview)
	{
		return true;
	}

	UEdGraphPin* PreviewedPin = NodeToPreview->FindPin(NodeToPreview->PreviewedPin);
	if (!PreviewedPin)
	{
		VOXEL_MESSAGE(Warning, "{0}: Invalid pin to preview", Compiler.GetSourceNode(NodeToPreview));
		return true;
	}

	FVoxelInstancedStruct PreviewHandler;
	for (const FVoxelPinPreviewSettings& PreviewSettings : NodeToPreview->PreviewSettings)
	{
		if (PreviewSettings.PinName == PreviewedPin->PinName)
		{
			ensure(!PreviewHandler.IsValid());
			PreviewHandler = PreviewSettings.PreviewHandler;
		}
	}
	if (!PreviewHandler.IsValid())
	{
		VOXEL_MESSAGE(Warning, "{0}: Invalid preview settings", Compiler.GetSourceNode(NodeToPreview));
		return true;
	}

	UVoxelGraphStructNode& SinkNode = Compiler.CreateNode<UVoxelGraphStructNode>(NodeToPreview, "Preview");
	SinkNode.Struct = FVoxelPreviewNode::StaticStruct();
	SinkNode.Struct.Get<FVoxelPreviewNode>().PreviewHandler = PreviewHandler;
	SinkNode.CreateNewGuid();
	SinkNode.PostPlacedNewNode();
	SinkNode.AllocateDefaultPins();

	// Promote will invalidate pin pointers
	{
		UEdGraphPin* ValuePin = SinkNode.FindPin(VOXEL_PIN_NAME(FVoxelPreviewNode, ValuePin));
		if (!ensure(ValuePin))
		{
			return true;
		}
		SinkNode.PromotePin(*ValuePin, PreviewedPin->PinType);
	}

	UEdGraphPin* ValuePin = SinkNode.FindPin(VOXEL_PIN_NAME(FVoxelPreviewNode, ValuePin));
	if (!ensure(ValuePin))
	{
		return true;
	}
	ValuePin->MakeLinkTo(PreviewedPin);

	return true;
}

bool FEditorCompilerPasses::AddToBufferNodes(FEditorCompiler& Compiler)
{
	for (UEdGraphNode* Node : Compiler.GetNodesCopy())
	{
		for (UEdGraphPin* InputPin : Node->Pins)
		{
			if (InputPin->Direction != EGPD_Input ||
				InputPin->LinkedTo.Num() == 0 ||
				!FVoxelPinType(InputPin->PinType).IsBuffer())
			{
				continue;
			}

			if (!ensure(InputPin->LinkedTo.Num() == 1))
			{
				return false;
			}

			UEdGraphPin* OutputPin = InputPin->LinkedTo[0];
			if (FVoxelPinType(OutputPin->PinType).IsBuffer())
			{
				continue;
			}

			UVoxelGraphStructNode& ToArrayNode = Compiler.CreateNode<UVoxelGraphStructNode>(Node, InputPin->GetName() + "_ToBuffer");
			ToArrayNode.Struct = TVoxelInstancedStruct<FVoxelNode>(*FVoxelFunctionNode::Make(FindUFunctionChecked(UVoxelBasicFunctionLibrary, ToBuffer)));
			ToArrayNode.CreateNewGuid();
			ToArrayNode.PostPlacedNewNode();
			ToArrayNode.AllocateDefaultPins();
			ToArrayNode.PromotePin(*ToArrayNode.GetInputPin(0), OutputPin->PinType);
			ToArrayNode.PromotePin(*ToArrayNode.GetOutputPin(0), InputPin->PinType);
			ToArrayNode.GetInputPin(0)->MakeLinkTo(OutputPin);
			ToArrayNode.GetOutputPin(0)->MakeLinkTo(InputPin);

			InputPin->BreakLinkTo(OutputPin);
		}
	}

	return true;
}

bool FEditorCompilerPasses::RemoveSplitPins(FEditorCompiler& Compiler)
{
	TMap<UEdGraphPin*, UVoxelGraphStructNode*> ParentPinToMakeBreakNodes;

	for (UEdGraphNode* Node : Compiler.GetNodesCopy())
	{
		const TArray<UEdGraphPin*> PinsCopy = Node->Pins;
		for (UEdGraphPin* SubPin : PinsCopy)
		{
			if (!SubPin->ParentPin)
			{
				continue;
			}

			UVoxelGraphStructNode*& MakeBreakNode = ParentPinToMakeBreakNodes.FindOrAdd(SubPin->ParentPin);

			if (!MakeBreakNode)
			{
				MakeBreakNode = &Compiler.CreateNode<UVoxelGraphStructNode>(Node, SubPin->ParentPin->GetName() + "_MakeBreak");

				if (SubPin->Direction == EGPD_Input)
				{
					const FVoxelNode* MakeNode = FVoxelNodeLibrary::FindMakeNode(SubPin->ParentPin->PinType);
					if (!ensure(MakeNode))
					{
						return false;
					}
					MakeBreakNode->Struct = *MakeNode;
				}
				else
				{
					check(SubPin->Direction == EGPD_Output);
					const FVoxelNode* BreakNode = FVoxelNodeLibrary::FindBreakNode(SubPin->ParentPin->PinType);
					if (!ensure(BreakNode))
					{
						return false;
					}
					MakeBreakNode->Struct = *BreakNode;
				}
				check(MakeBreakNode->Struct);

				MakeBreakNode->CreateNewGuid();
				MakeBreakNode->PostPlacedNewNode();
				MakeBreakNode->AllocateDefaultPins();

				if (SubPin->Direction == EGPD_Input)
				{
					ensure(MakeBreakNode->GetOutputPins().Num() == 1);
					UEdGraphPin* Pin = MakeBreakNode->GetOutputPin(0);

					FVoxelPinTypeSet Types;
					if (MakeBreakNode->CanPromotePin(*Pin, Types) &&
						ensure(Types.Contains(SubPin->ParentPin->PinType)))
					{
						MakeBreakNode->PromotePin(*Pin, SubPin->ParentPin->PinType);
						Pin = MakeBreakNode->GetOutputPin(0);
					}

					Pin->MakeLinkTo(SubPin->ParentPin);
				}
				else
				{
					check(SubPin->Direction == EGPD_Output);
					ensure(MakeBreakNode->GetInputPins().Num() == 1);
					UEdGraphPin* Pin = MakeBreakNode->GetInputPin(0);

					FVoxelPinTypeSet Types;
					if (MakeBreakNode->CanPromotePin(*Pin, Types) &&
						ensure(Types.Contains(SubPin->ParentPin->PinType)))
					{
						MakeBreakNode->PromotePin(*Pin, SubPin->ParentPin->PinType);
						Pin = MakeBreakNode->GetInputPin(0);
					}

					Pin->MakeLinkTo(SubPin->ParentPin);
				}
			}

			UEdGraphPin* NewPin = MakeBreakNode->FindPin(SubPin->GetName());
			if (!ensure(NewPin))
			{
				return false;
			}
			ensure(NewPin->LinkedTo.Num() == 0);

			NewPin->MovePersistentDataFromOldPin(*SubPin);

			SubPin->MarkAsGarbage();
			ensure(Node->Pins.Remove(SubPin) == 1);
		}
	}

	return true;
}

bool FEditorCompilerPasses::FixupLocalVariables(FEditorCompiler& Compiler)
{
	TMap<FGuid, UVoxelGraphLocalVariableDeclarationNode*> Declarations;
	TMap<UVoxelGraphLocalVariableDeclarationNode*, TArray<UVoxelGraphLocalVariableUsageNode*>> UsageDeclarationMapping;
	TMap<const FVoxelGraphParameter*, TArray<UVoxelGraphLocalVariableUsageNode*>> FreeUsages;

	for (UEdGraphNode* Node : Compiler.GetNodesCopy())
	{
		if (UVoxelGraphLocalVariableDeclarationNode* Declaration = Cast<UVoxelGraphLocalVariableDeclarationNode>(Node))
		{
			if (!Declaration->GetParameter())
			{
				VOXEL_MESSAGE(Error, "Invalid local variable node: {0}", Compiler.GetSourceNode(Node));
				return false;
			}

			if (Declarations.Contains(Declaration->Guid))
			{
				VOXEL_MESSAGE(Error, "Multiple local variable declarations not supported: {0} and {1}",
					Compiler.GetSourceNode(Declarations[Declaration->Guid]),
					Compiler.GetSourceNode(Node));
				return false;
			}

			if (const UVoxelGraphNode* LoopNode = Declaration->IsInLoop())
			{
				VOXEL_MESSAGE(Error, "Local variable is in loop: {0} with {1}",
					Compiler.GetSourceNode(Node),
					Compiler.GetSourceNode(LoopNode));
				return false;
			}

			Declarations.Add(Declaration->Guid, Declaration);
			UsageDeclarationMapping.FindOrAdd(Declaration);
		}
		else if (UVoxelGraphLocalVariableUsageNode* Usage = Cast<UVoxelGraphLocalVariableUsageNode>(Node))
		{
			const FVoxelGraphParameter* Parameter = Usage->GetParameter();
			if (!Parameter)
			{
				VOXEL_MESSAGE(Error, "Invalid local variable node: {0}", Compiler.GetSourceNode(Node));
				return false;
			}

			UVoxelGraphLocalVariableDeclarationNode* DeclarationNode = Usage->FindDeclaration();
			if (!DeclarationNode)
			{
				FreeUsages.FindOrAdd(Parameter).Add(Usage);
				continue;
			}

			UsageDeclarationMapping.FindOrAdd(DeclarationNode).Add(Usage);
		}
	}

	for (auto& It : UsageDeclarationMapping)
	{
		const UVoxelGraphLocalVariableDeclarationNode* Declaration = It.Key;

		UEdGraphPin* InputPin = Declaration->GetInputPin(0);
		for (UEdGraphPin* LinkedToPin : Declaration->GetOutputPin(0)->LinkedTo)
		{
			LinkedToPin->CopyPersistentDataFromOldPin(*InputPin);
			LinkedToPin->DefaultValue = InputPin->DefaultValue;
			LinkedToPin->DefaultObject = InputPin->DefaultObject;
		}

		for (const UVoxelGraphLocalVariableUsageNode* Usage : It.Value)
		{
			for (UEdGraphPin* LinkedToPin : Usage->GetOutputPin(0)->LinkedTo)
			{
				LinkedToPin->CopyPersistentDataFromOldPin(*InputPin);
				LinkedToPin->DefaultValue = InputPin->DefaultValue;
				LinkedToPin->DefaultObject = InputPin->DefaultObject;
			}
		}
	}

	for (auto& It : FreeUsages)
	{
		for (const UVoxelGraphLocalVariableUsageNode* Usage : It.Value)
		{
			if (It.Key->Type.HasPinDefaultValue())
			{
				for (UEdGraphPin* LinkedToPin : Usage->GetOutputPin(0)->LinkedTo)
				{
					It.Key->DefaultValue.ApplyToPinDefaultValue(*LinkedToPin);
				}
			}
		}
	}

	for (auto& It : UsageDeclarationMapping)
	{
		It.Key->DestroyNode();

		for (UVoxelGraphLocalVariableUsageNode* Node : It.Value)
		{
			Node->DestroyNode();
		}
	}

	for (auto& It : FreeUsages)
	{
		for (UVoxelGraphLocalVariableUsageNode* Node : It.Value)
		{
			Node->DestroyNode();
		}
	}

	return true;
}

END_VOXEL_NAMESPACE(Graph)