// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SculptVolume/VoxelSculptVolumeStorage.h"
#include "SculptVolume/VoxelSculptVolumeStorageData.h"
#include "SculptVolume/VoxelSculptVolumeStorageBakedData.h"

void UVoxelSculptVolumeStorage::Serialize(FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER_LLM();

	Super::Serialize(Ar);

	if (IsReloadActive())
	{
		return;
	}

	if (!Data)
	{
		Data = MakeVoxelShared<FVoxelSculptVolumeStorageData>(GetFName());
	}
	if (!BakedData)
	{
		BakedData = MakeVoxelShared<FVoxelSculptVolumeStorageBakedData>(GetFName());
	}

	FVoxelObjectUtilities::SerializeBulkData(this, BulkData, Ar, [&](FArchive& BulkDataAr)
	{
		Data->Serialize(BulkDataAr);
		BakedData->Serialize(BulkDataAr);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelSculptVolumeStorageData> UVoxelSculptVolumeStorage::GetData()
{
	if (!Data)
	{
		Data = MakeVoxelShared<FVoxelSculptVolumeStorageData>(GetFName());
	}
	return Data.ToSharedRef();
}

TSharedRef<FVoxelSculptVolumeStorageBakedData> UVoxelSculptVolumeStorage::GetBakedDataData()
{
	if (!BakedData)
	{
		BakedData = MakeVoxelShared<FVoxelSculptVolumeStorageBakedData>(GetFName());
	}
	return BakedData.ToSharedRef();
}