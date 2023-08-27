// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

BEGIN_VOXEL_NAMESPACE(Graph)

struct FStatsRow
{
	FText Header;
	FText Tooltip;
	TAttribute<FText> Value;
};

class SPreviewStats : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS() {};

	TArray<TSharedPtr<FStatsRow>> Rows;
	TSharedPtr<SListView<TSharedPtr<FStatsRow>>> RowsView;

	void Construct(const FArguments& Args);

private:
	TSharedRef<ITableRow> CreateRow(TSharedPtr<FStatsRow> StatsRow, const TSharedRef<STableViewBase>& OwnerTable) const;
};

END_VOXEL_NAMESPACE(Graph)