// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

struct FVoxelGraphToolkit;

class FVoxelGraphParameterSelectionCustomization : public IDetailCustomization
{
public:
	FVoxelGraphParameterSelectionCustomization(const TWeakPtr<FVoxelGraphToolkit>& Toolkit, const FGuid TargetParameterId)
		: WeakToolkit(Toolkit)
		, TargetParameterGuid(TargetParameterId)
	{
	}

	//~ Begin IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	//~ End IDetailCustomization Interface

private:
	TWeakPtr<FVoxelGraphToolkit> WeakToolkit;
	FGuid TargetParameterGuid;
};