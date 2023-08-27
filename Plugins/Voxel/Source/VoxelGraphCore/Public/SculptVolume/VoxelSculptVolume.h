// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelActor.h"
#include "VoxelSculptVolume.generated.h"

class UVoxelSculptVolumeStorage;
class FVoxelSculptVolumeStorageData;
struct FVoxelRuntimeParameter_SculptVolumeStorage;

UCLASS()
class VOXELGRAPHCORE_API AVoxelSculptVolume : public AVoxelActor
{
	GENERATED_BODY()

public:
	AVoxelSculptVolume();

	//~ Begin AVoxelActor Interface
	virtual FVoxelRuntimeParameters GetRuntimeParameters() const override;
	//~ End AVoxelActor Interface

	TSharedRef<FVoxelSculptVolumeStorageData> GetData() const;
	TSharedRef<FVoxelRuntimeParameter_SculptVolumeStorage> GetSculptVolumeParameter() const;

	UFUNCTION(BlueprintCallable, Category = "Voxel", meta = (ShowEditorButton))
	void ClearData();

private:
	UPROPERTY()
	TObjectPtr<UVoxelSculptVolumeStorage> InlineStorage;
};