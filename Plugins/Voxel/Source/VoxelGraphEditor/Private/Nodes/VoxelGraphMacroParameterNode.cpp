// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphMacroParameterNode.h"
#include "VoxelTransaction.h"
#include "VoxelGraphSchema.h"

void UVoxelGraphMacroParameterNode::PostPasteNode()
{
	Super::PostPasteNode();

	UVoxelGraph* Graph = GetTypedOuter<UVoxelGraph>();
	if (!ensure(Graph))
	{
		return;
	}

	// Add new member
	// Regenerate guid to be safe
	Guid = FGuid::NewGuid();
	CachedParameter.Guid = Guid;

	Graph->Parameters.Add(CachedParameter);

	ensure(Graph->Parameters.Last().Guid == CachedParameter.Guid);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelGraphMacroParameterNode::AllocateParameterPins(const FVoxelGraphParameter& Parameter)
{
	UEdGraphPin* Pin = CreatePin(
		Type == EVoxelGraphParameterType::Output ? EGPD_Input : EGPD_Output,
		Parameter.Type.GetEdGraphPinType(),
		STATIC_FNAME("Value"));

	Pin->PinFriendlyName = FText::FromName(GetParameter()->Name);

	if (!Parameter.Type.HasPinDefaultValue())
	{
		return;
	}

	if (Type == EVoxelGraphParameterType::Output)
	{
		Parameter.DefaultValue.ApplyToPinDefaultValue(*Pin);

		const FVoxelPinValue AutoDefaultValue = FVoxelPinValue(Parameter.Type.GetInnerType());
		Pin->AutogeneratedDefaultValue = AutoDefaultValue.ExportToString();
	}

	if (Type == EVoxelGraphParameterType::Input &&
		Parameter.bExposeInputDefaultAsPin)
	{
		UEdGraphPin* DefaultPin = CreatePin(
			EGPD_Input,
			Parameter.Type.GetEdGraphPinType(),
			STATIC_FNAME("Default"));

		Parameter.DefaultValue.ApplyToPinDefaultValue(*DefaultPin);

		DefaultPin->AutogeneratedDefaultValue = Parameter.DefaultValue.ExportToString();
	}
}

FText UVoxelGraphMacroParameterNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	switch (Type)
	{
	default: check(false);
	case EVoxelGraphParameterType::Input: return INVTEXT("INPUT");
	case EVoxelGraphParameterType::Output: return INVTEXT("OUTPUT");
	}
}

FLinearColor UVoxelGraphMacroParameterNode::GetNodeTitleColor() const
{
	return GetSchema()->GetPinTypeColor(GetParameterSafe().Type.GetEdGraphPinType());
}

void UVoxelGraphMacroParameterNode::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	ON_SCOPE_EXIT
	{
		Super::PinDefaultValueChanged(Pin);
	};

	if (Pin->Direction != EGPD_Input ||
		Pin->bOrphanedPin)
	{
		return;
	}

	FVoxelGraphParameter* Parameter = GetParameter();
	if (!ensure(Parameter))
	{
		return;
	}

	{
		UVoxelGraph& Graph = *GetTypedOuter<UVoxelGraph>();

		FVoxelTransaction Transaction(Graph);
		Transaction.SetProperty(FindFPropertyChecked_Impl<FVoxelPinValue>("Float"));
		Transaction.SetMemberProperty(FindFPropertyChecked(UVoxelGraph, Parameters));

		Parameter->DefaultValue = FVoxelPinValue::MakeFromPinDefaultValue(*Pin);
	}

	Pin = GetInputPin(0);
}

void UVoxelGraphMacroParameterNode::PinConnectionListChanged(UEdGraphPin* Pin)
{
	ON_SCOPE_EXIT
	{
		Super::PinConnectionListChanged(Pin);
	};

	if (Type != EVoxelGraphParameterType::Input ||
		Pin->Direction != EGPD_Input)
	{
		return;
	}

	FVoxelSystemUtilities::DelayedCall([Graph = GetTypedOuter<UVoxelGraph>()]
	{
		Graph->OnParametersChanged.Broadcast(UVoxelGraph::EParameterChangeType::Unknown);
	});
}

void UVoxelGraphMacroParameterNode::PostReconstructNode()
{
	if (GetInputPins().Num() > 0)
	{
		if (const FVoxelGraphParameter* Parameter = GetParameter())
		{
			if (UEdGraphPin* InputPin = GetInputPin(0))
			{
				Parameter->DefaultValue.ApplyToPinDefaultValue(*InputPin);
			}
		}
	}

	Super::PostReconstructNode();
}