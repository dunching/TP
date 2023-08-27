// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SVoxelGraphMembersGraphPaletteItem.h"
#include "SchemaActions/VoxelGraphMembersGraphSchemaAction.h"

BEGIN_VOXEL_NAMESPACE(Graph)

void SMembersGraphPaletteItem::Construct(const FArguments& InArgs, FCreateWidgetForActionData* const InCreateData)
{
	ActionPtr = InCreateData->Action;

	ChildSlot
	[
		SNew(SBox)
		.Padding(FMargin(0.f, -2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0.f, 2.f, 4.f, 2.f))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("GraphEditor.EventGraph_16x")))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(0.f)
			[
				CreateTextSlotWidget(InCreateData, false)
			]
		]
	];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMembersGraphPaletteItem::CreateTextSlotWidget(FCreateWidgetForActionData* const InCreateData, TAttribute<bool> bIsReadOnly)
{
	TSharedRef<SOverlay> DisplayWidget =
		SNew(SOverlay)
		+ SOverlay::Slot()
		.VAlign(VAlign_Center)
		[
			SNew(SVoxelDetailText)
			.Font(FAppStyle::GetFontStyle("NormalText"))
			.Text(this, &SMembersGraphPaletteItem::GetDisplayText)
		];

	return DisplayWidget;
}

FText SMembersGraphPaletteItem::GetDisplayText() const
{
	const TSharedPtr<FVoxelGraphMembersGraphSchemaAction> Action = GetAction();
	if (!ensure(Action))
	{
		return {};
	}

	return FText::FromString(Action->GetName());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FVoxelGraphMembersGraphSchemaAction> SMembersGraphPaletteItem::GetAction() const
{
	const TSharedPtr<FEdGraphSchemaAction> Action = ActionPtr.Pin();
	if (!Action)
	{
		return nullptr;
	}

	return StaticCastSharedPtr<FVoxelGraphMembersGraphSchemaAction>(Action);
}

END_VOXEL_NAMESPACE(Graph)