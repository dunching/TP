// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelDeveloperSettings.h"
#include "VoxelLandmassSettings.generated.h"

UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Voxel Landmass"))
class VOXELLANDMASS_API UVoxelLandmassSettings : public UVoxelDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoxelLandmassSettings()
	{
		SectionName = "Voxel Landmass";
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Mesh Brush")
	float BaseVoxelSize = 20.f;
};