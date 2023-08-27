// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEdGraph.h"
#include "VoxelGraphToolkit.h"
#include "VoxelGraphSchemaBase.h"

TSharedPtr<FVoxelGraphToolkit> UVoxelEdGraph::GetGraphToolkit() const
{
	return WeakToolkit.Pin();
}

void UVoxelEdGraph::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	DelayOnGraphChangedScopeStack.Add(MakeVoxelShared<FVoxelGraphDelayOnGraphChangedScope>());
}

void UVoxelEdGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	// After undo/redo toolkit may be null
	if (WeakToolkit.IsValid())
	{
		UVoxelGraphSchemaBase::OnGraphChanged(this);
	}

	if (ensure(DelayOnGraphChangedScopeStack.Num() > 0))
	{
		DelayOnGraphChangedScopeStack.Pop();
	}
}