// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSculptToolkit.h"
#include "VoxelSculptEdMode.h"

void FVoxelSculptToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	SetCurrentPalette("Tools");
}

FName FVoxelSculptToolkit::GetToolkitFName() const
{
	return GetDefault<UVoxelSculptEdMode>()->GetModeInfo().ID;
}

FText FVoxelSculptToolkit::GetBaseToolkitName() const
{
	return GetDefault<UVoxelSculptEdMode>()->GetModeInfo().Name;
}

FText FVoxelSculptToolkit::GetActiveToolDisplayName() const
{
	return INVTEXT("FVoxelSculptToolkit::GetActiveToolDisplayName");
}

FText FVoxelSculptToolkit::GetActiveToolMessage() const
{
	return INVTEXT("FVoxelSculptToolkit::GetActiveToolMessage");
}

void FVoxelSculptToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add("Tools");
}