// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelTransformRef.h"
#include "VoxelRuntimeParameter.h"
#include "VoxelSculptVolumeStorage.generated.h"

class FVoxelSculptVolumeStorageData;
class FVoxelSculptVolumeStorageBakedData;

UCLASS()
class UVoxelSculptVolumeStorage : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	TSharedRef<FVoxelSculptVolumeStorageData> GetData();
	TSharedRef<FVoxelSculptVolumeStorageBakedData> GetBakedDataData();

private:
	FByteBulkData BulkData;
	TSharedPtr<FVoxelSculptVolumeStorageData> Data;
	TSharedPtr<FVoxelSculptVolumeStorageBakedData> BakedData;
};

USTRUCT()
struct FVoxelRuntimeParameter_SculptVolumeStorage : public FVoxelRuntimeParameter
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	TOptional<FVoxelTransformRef> SurfaceToWorldOverride;
	TSharedPtr<FVoxelSculptVolumeStorageData> Data;
	TSharedPtr<FVoxelSculptVolumeStorageBakedData> BakedData;
};