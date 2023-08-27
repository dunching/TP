// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphInputDragDropAction.h"

#include "VoxelGraph.h"
#include "VoxelGraphSchema.h"
#include "VoxelGraphVisuals.h"
#include "Nodes/VoxelGraphParameterNodeBase.h"
#include "Nodes/VoxelGraphLocalVariableNode.h"
#include "Nodes/VoxelGraphMacroParameterNode.h"

BEGIN_VOXEL_NAMESPACE(Graph)

void FInputDragDropAction::HoverTargetChanged()
{
	UVoxelGraph* Graph = WeakGraph.Get();
	if (!ensure(Graph))
	{
		return;
	}

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(ParameterGuid);
	if (!ensure(Parameter))
	{
		return;
	}

	if (Parameter->ParameterType == EVoxelGraphParameterType::Parameter)
	{
		if (const UEdGraphPin* Pin = GetHoveredPin())
		{
			if (Pin->bOrphanedPin)
			{
				SetFeedbackMessageError("Cannot make connection to orphaned pin " + Pin->PinName.ToString() + "");
				return;
			}
			else
			{
				if (Pin->Direction != EGPD_Input)
				{
					SetFeedbackMessageError("Input parameter '" + Parameter->Name.ToString() + "' cannot be connected to output pin");
					return;
				}

				if (Pin->PinType == Parameter->Type &&
					!Pin->bNotConnectable)
				{
					SetFeedbackMessageOK("Make " + Pin->PinName.ToString() + " = " + Parameter->Name.ToString() + "");
				}
				else
				{
					SetFeedbackMessageError("The type of '" + Parameter->Name.ToString() + "' parameter is not compatible with " + Pin->PinName.ToString() + " pin");
				}
				return;
			}
		}

		if (UVoxelGraphParameterNodeBase* Node = Cast<UVoxelGraphParameterNodeBase>(GetHoveredNode()))
		{
			const FVoxelGraphParameter& ParameterNode = Node->GetParameterSafe();

			bool bCanChangeNode = true;
			if (const UVoxelGraphMacroParameterNode* MacroParameterNode = Cast<UVoxelGraphMacroParameterNode>(Node))
			{
				bCanChangeNode = MacroParameterNode->Type == EVoxelGraphParameterType::Input;
			}
			else if (Cast<UVoxelGraphLocalVariableDeclarationNode>(Node))
			{
				bCanChangeNode = false;
			}

			if (bCanChangeNode)
			{
				bool bBreakLinks = false;
				if (Parameter->Type != ParameterNode.Type)
				{
					for (const UEdGraphPin* Pin : Node->GetAllPins())
					{
						if (Pin->LinkedTo.Num() > 0)
						{
							bBreakLinks = true;
							break;
						}
					}
				}

				SetFeedbackMessageOK("Change node to read '" + Parameter->Name.ToString() + "'" + (bBreakLinks ? ". WARNING: This will break links!" : ""));
				return;
			}
		}
	}

	FVoxelMembersBaseDragDropAction::HoverTargetChanged();

	if (Parameter->ParameterType == EVoxelGraphParameterType::Output)
	{
		SetFeedbackMessageError("Output parameter '" + Parameter->Name.ToString() + "' is already used in graph");
		return;
	}

	if (Parameter->ParameterType == EVoxelGraphParameterType::Input)
	{
		SetFeedbackMessageError("Input parameter '" + Parameter->Name.ToString() + "' is already used in graph");
	}
}

FReply FInputDragDropAction::DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition)
{
	UVoxelGraph* Graph = WeakGraph.Get();
	if (!ensure(Graph))
	{
		return FReply::Handled();
	}

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(ParameterGuid);
	if (!ensure(Parameter))
	{
		return FReply::Handled();
	}

	if (Parameter->ParameterType != EVoxelGraphParameterType::Parameter)
	{
		return FReply::Handled();
	}

	UEdGraphPin* TargetPin = GetHoveredPin();
	if (!ensure(TargetPin))
	{
		return FReply::Unhandled();
	}

	if (TargetPin->bOrphanedPin ||
		TargetPin->PinType != Parameter->Type ||
		TargetPin->Direction == EGPD_Output ||
		TargetPin->bNotConnectable)
	{
		return FReply::Handled();
	}

	FVoxelGraphSchemaAction_NewParameterUsage Action;
	Action.Guid = ParameterGuid;
	Action.ParameterType = Parameter->ParameterType;
	Action.PerformAction(TargetPin->GetOwningNode()->GetGraph(), TargetPin, GraphPosition, true);

	return FReply::Handled();
}

FReply FInputDragDropAction::DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition)
{
	UVoxelGraph* Graph = WeakGraph.Get();
	if (!ensure(Graph))
	{
		return FReply::Handled();
	}

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(ParameterGuid);
	if (!ensure(Parameter))
	{
		return FReply::Handled();
	}

	if (Parameter->ParameterType != EVoxelGraphParameterType::Parameter)
	{
		return FReply::Handled();
	}

	UEdGraphNode* Node = GetHoveredNode();
	if (!ensure(Node))
	{
		return FReply::Unhandled();
	}

	if (UVoxelGraphParameterNodeBase* ParameterNode = Cast<UVoxelGraphParameterNodeBase>(Node))
	{
		const FVoxelGraphParameter& TargetParameter = ParameterNode->GetParameterSafe();

		if (const UVoxelGraphMacroParameterNode* MacroParameterNode = Cast<UVoxelGraphMacroParameterNode>(ParameterNode))
		{
			if (MacroParameterNode->Type != EVoxelGraphParameterType::Input)
			{
				return FReply::Unhandled();
			}
		}
		else if (ParameterNode->IsA<UVoxelGraphLocalVariableDeclarationNode>())
		{
			return FReply::Unhandled();
		}

		const FVoxelTransaction Transaction(Graph, "Replace parameter");

		FVoxelGraphSchemaAction_NewParameterUsage Action;
		Action.Guid = ParameterGuid;
		Action.ParameterType = EVoxelGraphParameterType::Parameter;
		const UVoxelGraphNodeBase* NewNode = Cast<UVoxelGraphNodeBase>(Action.PerformAction(Node->GetGraph(), nullptr, FVector2D(Node->NodePosX, Node->NodePosY), true));

		if (Parameter->Type == TargetParameter.Type)
		{
			for (int32 Index = 0; Index < FMath::Min(ParameterNode->GetOutputPins().Num(), NewNode->GetOutputPins().Num()); Index++)
			{
				NewNode->GetOutputPin(Index)->CopyPersistentDataFromOldPin(*ParameterNode->GetOutputPin(Index));
			}
		}

		Node->GetGraph()->RemoveNode(Node);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply FInputDragDropAction::DroppedOnPanel(const TSharedRef<SWidget>& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& EdGraph)
{
	UVoxelGraph* Graph = WeakGraph.Get();
	if (!ensure(Graph))
	{
		return FReply::Handled();
	}

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(ParameterGuid);
	if (!ensure(Parameter))
	{
		return FReply::Handled();
	}

	if (Parameter->ParameterType != EVoxelGraphParameterType::Parameter)
	{
		return FReply::Handled();
	}

	FVoxelGraphSchemaAction_NewParameterUsage Action;
	Action.Guid = ParameterGuid;
	Action.ParameterType = Parameter->ParameterType;
	Action.PerformAction(&EdGraph, nullptr, GraphPosition, true);

	return FReply::Handled();
}

void FInputDragDropAction::GetDefaultStatusSymbol(const FSlateBrush*& PrimaryBrushOut, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut) const
{
	FGraphSchemaActionDragDropAction::GetDefaultStatusSymbol(PrimaryBrushOut, IconColorOut, SecondaryBrushOut, SecondaryColorOut);

	UVoxelGraph* Graph = WeakGraph.Get();
	if (!ensure(Graph))
	{
		return;
	}

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(ParameterGuid);
	if (!ensure(Parameter))
	{
		return;
	}

	PrimaryBrushOut = FVoxelGraphVisuals::GetPinIcon(Parameter->Type).GetIcon();
	IconColorOut = FVoxelGraphVisuals::GetPinColor(Parameter->Type);
}

END_VOXEL_NAMESPACE(Graph)