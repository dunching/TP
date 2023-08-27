// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SVoxelGraphMembersMacroLibraryPaletteItem.h"

#include "SchemaActions/VoxelGraphMembersMacroLibrarySchemaAction.h"

BEGIN_VOXEL_NAMESPACE(Graph)

void SMacroLibraryPaletteItem::Construct(const FArguments& InArgs, FCreateWidgetForActionData* const InCreateData)
{
	ActionPtr = InCreateData->Action;

	ChildSlot
	[
		SNew(SBox)
		.Padding(0.f, -2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0.f, 2.f, 4.f, 2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("GraphEditor.Macro_16x")))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(0.f)
			[
				CreateTextSlotWidget(InCreateData, false)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.f, 0.f)
			[
				CreateVisibilityWidget()
			]
		]
	];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMacroLibraryPaletteItem::CreateTextSlotWidget(FCreateWidgetForActionData* const InCreateData, TAttribute<bool> bIsReadOnly)
{
	if (InCreateData->bHandleMouseButtonDown)
	{
		MouseButtonDownDelegate = InCreateData->MouseButtonDownDelegate;
	}

	TSharedRef<SOverlay> DisplayWidget =
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(InlineRenameWidget, SInlineEditableTextBlock)
			.Text(this, &SMacroLibraryPaletteItem::GetDisplayText)
			.HighlightText(InCreateData->HighlightText)
			.OnVerifyTextChanged(this, &SMacroLibraryPaletteItem::OnNameTextVerifyChanged)
			.OnTextCommitted(this, &SMacroLibraryPaletteItem::OnNameTextCommitted)
			.IsSelected(InCreateData->IsRowSelectedDelegate)
			.IsReadOnly(bIsReadOnly)
		];

	InCreateData->OnRenameRequest->BindSP(InlineRenameWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);

	return DisplayWidget;
}

void SMacroLibraryPaletteItem::OnNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	const TSharedPtr<FVoxelGraphMembersMacroLibrarySchemaAction> Action = GetAction();
	if (!ensure(Action))
	{
		return;
	}

	Action->SetName(NewText.ToString());
}

FText SMacroLibraryPaletteItem::GetDisplayText() const
{
	const TSharedPtr<FVoxelGraphMembersMacroLibrarySchemaAction> Action = GetAction();
	if (!ensure(Action))
	{
		return {};
	}

	return FText::FromString(Action->GetName());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMacroLibraryPaletteItem::CreateVisibilityWidget() const
{
	TSharedRef<SWidget> Button = PropertyCustomizationHelpers::MakeVisibilityButton(
		FOnClicked::CreateLambda([this]() -> FReply
		{
			const TSharedPtr<FVoxelGraphMembersMacroLibrarySchemaAction> Action = GetAction();
			if (!ensure(Action))
			{
				return FReply::Handled();
			}

			Action->ToggleMacroVisibility();
			return FReply::Handled();
		}),
		{},
		MakeAttributeLambda([this]
		{
			const TSharedPtr<FVoxelGraphMembersMacroLibrarySchemaAction> Action = GetAction();
			if (!ensure(Action))
			{
				return false;
			}

			return Action->IsMacroVisible();
		}));

	Button->SetToolTipText(INVTEXT("Expose Macro"));

	return Button;
}

TSharedPtr<FVoxelGraphMembersMacroLibrarySchemaAction> SMacroLibraryPaletteItem::GetAction() const
{
	const TSharedPtr<FEdGraphSchemaAction> Action = ActionPtr.Pin();
	if (!Action)
	{
		return nullptr;
	}

	return StaticCastSharedPtr<FVoxelGraphMembersMacroLibrarySchemaAction>(Action);
}

END_VOXEL_NAMESPACE(Graph)