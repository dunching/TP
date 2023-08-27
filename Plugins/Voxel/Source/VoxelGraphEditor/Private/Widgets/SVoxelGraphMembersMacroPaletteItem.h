// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "SGraphPalette.h"

struct FVoxelGraphMembersMacroSchemaAction;

BEGIN_VOXEL_NAMESPACE(Graph)

class SMembers;

class SMacroPaletteItem : public SGraphPaletteItem
{
public:
	VOXEL_SLATE_ARGS()
	{
	};

	void Construct(const FArguments& InArgs, FCreateWidgetForActionData* const InCreateData);

protected:
	//~ Begin SGraphPaletteItem Interface
	virtual TSharedRef<SWidget> CreateTextSlotWidget(FCreateWidgetForActionData* const InCreateData, TAttribute<bool> bIsReadOnly) override;
	virtual void OnNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit) override;
	virtual FText GetDisplayText() const override;
	//~ End SGraphPaletteItem Interface

private:
	TSharedPtr<FVoxelGraphMembersMacroSchemaAction> GetAction() const;
};

END_VOXEL_NAMESPACE(Graph)