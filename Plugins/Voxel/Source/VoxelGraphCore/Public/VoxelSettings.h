// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelChannel.h"
#include "VoxelDeveloperSettings.h"
#include "VoxelSettings.generated.h"

UCLASS(config = Engine, DefaultConfig, meta = (DisplayName = "Voxel Plugin"))
class VOXELGRAPHCORE_API UVoxelSettings : public UVoxelDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoxelSettings()
	{
		SectionName = "Voxel Plugin";
	}

	// Number of threads allocated for the voxel background processing. Setting it too high may impact performance
	// The threads are shared across all voxel worlds
	// Can be set using voxel.NumThreads
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = 1, ConsoleVariable = "voxel.NumThreads"))
	int32 NumberOfThreads = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Config")
	TArray<FVoxelChannelExposedDefinition> GlobalChannels;

	// Number of frames to collect the average frame rate from
	UPROPERTY(Config, EditAnywhere, Category = "Safety", meta = (ClampMin = 2))
	int32 FramesToAverage = 128;

	// Minimum average FPS allowed in specified number of frames
	UPROPERTY(Config, EditAnywhere, Category = "Safety", meta = (ClampMin = 1))
	int32 MinFPS = 8;

	void UpdateChannels();

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface
};