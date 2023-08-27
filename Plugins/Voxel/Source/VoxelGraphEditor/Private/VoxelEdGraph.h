// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelEdGraph.generated.h"

class SGraphEditor;
struct FVoxelGraphToolkit;
struct FVoxelGraphDelayOnGraphChangedScope;

UCLASS()
class UVoxelEdGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	TWeakPtr<FVoxelGraphToolkit> WeakToolkit;

	TSharedPtr<FVoxelGraphToolkit> GetGraphToolkit() const;

	//~ Begin UObject interface
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
private:
	TArray<TSharedPtr<FVoxelGraphDelayOnGraphChangedScope>> DelayOnGraphChangedScopeStack;
};