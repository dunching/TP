// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphParameterNode.h"
#include "VoxelTransaction.h"
#include "VoxelGraphSchema.h"
#include "VoxelGraphToolkit.h"

void UVoxelGraphParameterNode::PostPasteNode()
{
	Super::PostPasteNode();

	UVoxelGraph* Graph = GetTypedOuter<UVoxelGraph>();
	if (!ensure(Graph))
	{
		return;
	}

	if (Graph->Parameters.FindByKey(Guid))
	{
		return;
	}

	const FVoxelGraphParameter* Parameter = Graph->Parameters.FindByKey(CachedParameter.Name);
	if (Parameter &&
		Parameter->Type == CachedParameter.Type &&
		Parameter->ParameterType == EVoxelGraphParameterType::Parameter)
	{
		// Update Guid
		Guid = Parameter->Guid;
		return;
	}

	// Add new parameter
	// Regenerate guid to be safe
	Guid = FGuid::NewGuid();
	CachedParameter.Guid = Guid;

	Graph->Parameters.Add(CachedParameter);

	ensure(Graph->Parameters.Last().Guid == CachedParameter.Guid);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelGraphParameterNode::AllocateParameterPins(const FVoxelGraphParameter& Parameter)
{
	FVoxelPinType Type = Parameter.Type;
	if (bIsBuffer)
	{
		Type = Type.GetBufferType();
	}

	UEdGraphPin* Pin = CreatePin(
		EGPD_Output,
		Type.GetEdGraphPinType(),
		STATIC_FNAME("Value"));

	Pin->PinFriendlyName = FText::FromName(Parameter.Name);
}

FText UVoxelGraphParameterNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType != ENodeTitleType::FullTitle)
	{
		return {};
	}

	return FText::FromName(GetParameterSafe().Name);
}

FLinearColor UVoxelGraphParameterNode::GetNodeTitleColor() const
{
	return GetSchema()->GetPinTypeColor(GetParameterSafe().Type.GetEdGraphPinType());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UVoxelGraphParameterNode::CanPromotePin(const UEdGraphPin& Pin, FVoxelPinTypeSet& OutTypes) const
{
	const FVoxelGraphParameter* Parameter = GetParameter();
	if (!Parameter)
	{
		return false;
	}

	OutTypes.Add(Parameter->Type.GetInnerType());
	OutTypes.Add(Parameter->Type.GetBufferType());

	return true;
}

void UVoxelGraphParameterNode::PromotePin(UEdGraphPin& Pin, const FVoxelPinType& NewType)
{
	Modify();

	ensure(bIsBuffer != NewType.IsBuffer());
	bIsBuffer = NewType.IsBuffer();

	ReconstructNode(false);
}