// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SVoxelBlueprintGraphNode.h"

void SVoxelBlueprintGraphNode::Construct(const FArguments& InArgs, UK2Node_VoxelBaseNode* InNode)
{
	GraphNode = InNode;

	SetCursor( EMouseCursor::CardinalCross );

	UpdateGraphNode();
}

void SVoxelBlueprintGraphNode::CreatePinWidgets()
{
	static const FSlateBrush* OuterIcon = FAppStyle::GetBrush(STATIC_FNAME("Kismet.VariableList.PromotableTypeOuterIcon"));
	static const FSlateBrush* InnerIcon = FAppStyle::GetBrush(STATIC_FNAME("Kismet.VariableList.PromotableTypeInnerIcon"));

	static const FSlateBrush* BufferInnerIcon = FVoxelEditorStyle::GetBrush("Pin.Buffer.Promotable.Inner");
	static const FSlateBrush* BufferOuterIcon = FVoxelEditorStyle::GetBrush("Pin.Buffer.Promotable.Outer");

	SGraphNodeK2Base::CreatePinWidgets();

	TSet<TSharedRef<SWidget>> AllPins;
	GetPins(AllPins);

	for (const TSharedRef<SWidget>& Widget : AllPins)
	{
		if (const TSharedPtr<SGraphPin>& PinWidget = StaticCastSharedRef<SGraphPin>(Widget))
		{
			const UEdGraphPin* Pin = PinWidget->GetPinObj();

			if (!Pin ||
				Pin->ParentPin ||
				!GetNode().IsPinWildcard(*Pin))
			{
				continue;
			}

			const TSharedPtr<SLayeredImage> PinImage = StaticCastSharedPtr<SLayeredImage>(PinWidget->GetPinImageWidget());
			if (!PinImage)
			{
				continue;
			}

			if (Pin->PinType.IsArray())
			{
				PinImage->SetLayerBrush(0, BufferOuterIcon);
				PinImage->AddLayer(BufferInnerIcon, GetDefault<UGraphEditorSettings>()->WildcardPinTypeColor);
			}
			else
			{
				PinImage->SetLayerBrush(0, OuterIcon);
				PinImage->AddLayer(InnerIcon, GetDefault<UGraphEditorSettings>()->WildcardPinTypeColor);
			}
		}
	}
}