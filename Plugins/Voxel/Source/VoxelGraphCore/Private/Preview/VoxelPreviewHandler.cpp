// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "Preview/VoxelPreviewHandler.h"

#include "VoxelQuery.h"

const TArray<const FVoxelPreviewHandler*>& FVoxelPreviewHandler::GetHandlers()
{
	static TArray<const FVoxelPreviewHandler*> Handlers;
	if (Handlers.Num() == 0)
	{
		for (UScriptStruct* Struct : GetDerivedStructs<FVoxelPreviewHandler>())
		{
			Handlers.Add(TVoxelInstancedStruct<FVoxelPreviewHandler>(Struct).Release());
		}
	}
	return Handlers;
}

void FVoxelPreviewHandler::BuildStats(const FAddStat& AddStat)
{
	AddStat(
		"Position",
		"The position currently being previewed",
		MakeWeakPtrLambda(this, [this]
		{
			return CurrentPosition.ToString();
		}));
}

void FVoxelPreviewHandler::UpdateStats(const FVector2D& MousePosition)
{
	const FMatrix PixelToWorld = QueryContext->RuntimeInfo->GetLocalToWorld().Get_NoDependency();
	CurrentPosition = PixelToWorld.TransformPosition(FVector(MousePosition.X, MousePosition.Y, 0.f));
}