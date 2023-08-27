// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphLocalVariableDragDropAction.h"

#include "VoxelGraph.h"
#include "VoxelGraphSchema.h"
#include "VoxelGraphVisuals.h"
#include "Nodes/VoxelGraphParameterNodeBase.h"
#include "Nodes/VoxelGraphLocalVariableNode.h"
#include "Nodes/VoxelGraphMacroParameterNode.h"

BEGIN_VOXEL_NAMESPACE(Graph)

void FLocalVariableDragDropAction::HoverTargetChanged()
{
	UVoxelGraph* Graph = WeakGraph.Get();
	if (!ensure(Graph))
	{
		return;
	}

	if (GetHoveredGraph() &&
		GetHoveredGraph() != Graph->MainEdGraph)
	{
		SetFeedbackMessageError("Local variable can be used only in owner graph");
		return;
	}

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(ParameterGuid);
	if (!ensure(Parameter))
	{
		return;
	}

	if (!ensure(Parameter->ParameterType == EVoxelGraphParameterType::LocalVariable))
	{
		return;
	}

	if (const UEdGraphPin* Pin = GetHoveredPin())
	{
		if (Pin->bOrphanedPin)
		{
			SetFeedbackMessageError("Cannot make connection to orphaned pin " + Pin->PinName.ToString() + "");
			return;
		}

		if (Pin->PinType == Parameter->Type &&
			!Pin->bNotConnectable)
		{
			if (Pin->Direction == EGPD_Input)
			{
				SetFeedbackMessageOK("Make " + Pin->PinName.ToString() + " = " + Parameter->Name.ToString() + "");
			}
			else
			{
				SetFeedbackMessageOK("Make " + Parameter->Name.ToString() + " = " + Pin->PinName.ToString() + "");
			}
		}
		else
		{
			SetFeedbackMessageError("The type of '" + Parameter->Name.ToString() + "' local variable is not compatible with " + Pin->PinName.ToString() + " pin");
		}
		return;
	}

	if (UVoxelGraphParameterNodeBase* ParameterNode = Cast<UVoxelGraphParameterNodeBase>(GetHoveredNode()))
	{
		const FVoxelGraphParameter& TargetParameter = ParameterNode->GetParameterSafe();

		bool bWrite = false;
		if (const UVoxelGraphMacroParameterNode* MacroParameterNode = Cast<UVoxelGraphMacroParameterNode>(ParameterNode))
		{
			bWrite = MacroParameterNode->Type == EVoxelGraphParameterType::Output;
		}
		else if (ParameterNode->IsA<UVoxelGraphLocalVariableDeclarationNode>())
		{
			bWrite = true;
		}

		bool bBreakLinks = false;
		if (Parameter->Type != TargetParameter.Type)
		{
			for (const UEdGraphPin* Pin : ParameterNode->GetAllPins())
			{
				if (Pin->LinkedTo.Num() > 0)
				{
					bBreakLinks = true;
					break;
				}
			}
		}

		SetFeedbackMessageOK("Change node to " + FString(bWrite ? "write" : "read") + " '" + Parameter->Name.ToString() + "'" + (bBreakLinks ? ". WARNING: This will break links!" : ""));
		return;
	}

	FVoxelMembersBaseDragDropAction::HoverTargetChanged();
}

FReply FLocalVariableDragDropAction::DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition)
{
	UVoxelGraph* Graph = WeakGraph.Get();
	if (!ensure(Graph))
	{
		return FReply::Handled();
	}

	if (Graph->MainEdGraph != GetHoveredGraph())
	{
		return FReply::Handled();
	}

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(ParameterGuid);
	if (!ensure(Parameter))
	{
		return FReply::Handled();
	}

	if (!ensure(Parameter->ParameterType == EVoxelGraphParameterType::LocalVariable))
	{
		return FReply::Handled();
	}

	UEdGraphPin* Pin = GetHoveredPin();
	if (!ensure(Pin))
	{
		return FReply::Unhandled();
	}

	if (Pin->bOrphanedPin ||
		Pin->PinType != Parameter->Type ||
		Pin->bNotConnectable)
	{
		return FReply::Handled();
	}

	FVoxelGraphSchemaAction_NewParameterUsage Action;
	Action.Guid = ParameterGuid;
	Action.ParameterType = Parameter->ParameterType;
	Action.bDeclaration = Pin->Direction == EGPD_Output;
	Action.PerformAction(Pin->GetOwningNode()->GetGraph(), Pin, GraphPosition, true);

	return FReply::Handled();
}

FReply FLocalVariableDragDropAction::DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition)
{
	UVoxelGraph* Graph = WeakGraph.Get();
	if (!ensure(Graph))
	{
		return FReply::Handled();
	}

	if (Graph->MainEdGraph != GetHoveredGraph())
	{
		return FReply::Handled();
	}

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(ParameterGuid);
	if (!ensure(Parameter))
	{
		return FReply::Handled();
	}

	if (!ensure(Parameter->ParameterType == EVoxelGraphParameterType::LocalVariable))
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

		bool bWrite = false;
		if (const UVoxelGraphMacroParameterNode* MacroParameterNode = Cast<UVoxelGraphMacroParameterNode>(ParameterNode))
		{
			bWrite = MacroParameterNode->Type == EVoxelGraphParameterType::Output;
		}
		else if (Cast<UVoxelGraphLocalVariableDeclarationNode>(ParameterNode))
		{
			bWrite = true;
		}

		const FVoxelTransaction Transaction(Graph, "Replace local variable");

		FVoxelGraphSchemaAction_NewParameterUsage Action;
		Action.Guid = ParameterGuid;
		Action.ParameterType = EVoxelGraphParameterType::LocalVariable;
		Action.bDeclaration = bWrite;

		const UVoxelGraphNodeBase* NewNode = Cast<UVoxelGraphNodeBase>(Action.PerformAction(Node->GetGraph(), nullptr, FVector2D(Node->NodePosX, Node->NodePosY), true));
		if (Parameter->Type == TargetParameter.Type)
		{
			for (int32 Index = 0; Index < FMath::Min(ParameterNode->GetInputPins().Num(), NewNode->GetInputPins().Num()); Index++)
			{
				NewNode->GetInputPin(Index)->CopyPersistentDataFromOldPin(*ParameterNode->GetInputPin(Index));
			}
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

FReply FLocalVariableDragDropAction::DroppedOnPanel(const TSharedRef<SWidget>& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& EdGraph)
{
	UVoxelGraph* Graph = WeakGraph.Get();
	if (!ensure(Graph))
	{
		return FReply::Handled();
	}

	if (Graph->MainEdGraph != GetHoveredGraph())
	{
		return FReply::Handled();
	}

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(ParameterGuid);
	if (!ensure(Parameter))
	{
		return FReply::Handled();
	}

	if (!ensure(Parameter->ParameterType == EVoxelGraphParameterType::LocalVariable))
	{
		return FReply::Handled();
	}

	const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
	const bool bModifiedKeysActive = ModifierKeys.IsControlDown() || ModifierKeys.IsAltDown();
	const bool bAutoCreateGetter = bModifiedKeysActive ? ModifierKeys.IsControlDown() : bControlDrag;
	const bool bAutoCreateSetter = bModifiedKeysActive ? ModifierKeys.IsAltDown() : bAltDrag;

	if (bAutoCreateGetter)
	{
		CreateVariable(true, ParameterGuid, &EdGraph, GraphPosition, Parameter->ParameterType);
	}
	else if (bAutoCreateSetter)
	{
		CreateVariable(false, ParameterGuid, &EdGraph, GraphPosition, Parameter->ParameterType);
	}
	else
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.BeginSection("BPVariableDroppedOn", FText::FromName(Parameter->Name) );
		{
			MenuBuilder.AddMenuEntry(
				FText::FromString("Get " + Parameter->Name.ToString()),
				FText::FromString("Create Getter for local variable '" + Parameter->Name.ToString() + "'\n(Ctrl-drag to automatically create a getter)"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&FLocalVariableDragDropAction::CreateVariable, true, ParameterGuid, &EdGraph, GraphPosition, Parameter->ParameterType), FCanExecuteAction())
			);

			MenuBuilder.AddMenuEntry(
				FText::FromString("Set " + Parameter->Name.ToString()),
				FText::FromString("Create Setter for local variable '" + Parameter->Name.ToString() + "'\n(Alt-drag to automatically create a setter)"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&FLocalVariableDragDropAction::CreateVariable, false, ParameterGuid, &EdGraph, GraphPosition, Parameter->ParameterType), FCanExecuteAction())
			);
		}
		MenuBuilder.EndSection();

		FSlateApplication::Get().PushMenu(
			Panel,
			FWidgetPath(),
			MenuBuilder.MakeWidget(),
			ScreenPosition,
			FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu)
		);
	}

	return FReply::Handled();
}

void FLocalVariableDragDropAction::GetDefaultStatusSymbol(const FSlateBrush*& PrimaryBrushOut, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut) const
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FLocalVariableDragDropAction::CreateVariable(bool bGetter, const FGuid NewParameterGuid, UEdGraph* Graph, const FVector2D GraphPosition, const EVoxelGraphParameterType ParameterType)
{
	FVoxelGraphSchemaAction_NewParameterUsage Action;
	Action.Guid = NewParameterGuid;
	Action.ParameterType = ParameterType;
	Action.bDeclaration = !bGetter;
	Action.PerformAction(Graph, nullptr, GraphPosition, true);
}

END_VOXEL_NAMESPACE(Graph)