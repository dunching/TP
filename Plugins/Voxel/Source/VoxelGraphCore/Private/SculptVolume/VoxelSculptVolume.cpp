// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SculptVolume/VoxelSculptVolume.h"
#include "SculptVolume/VoxelSculptVolumeStorage.h"
#include "SculptVolume/VoxelSculptVolumeStorageData.h"

AVoxelSculptVolume::AVoxelSculptVolume()
{
	InlineStorage = CreateDefaultSubobject<UVoxelSculptVolumeStorage>("InlineStorage");

	SetActorScale3D(FVector(100.f));
}

FVoxelRuntimeParameters AVoxelSculptVolume::GetRuntimeParameters() const
{
	FVoxelRuntimeParameters Parameters = Super::GetRuntimeParameters();
	Parameters.Add(GetSculptVolumeParameter());
	return Parameters;
}

TSharedRef<FVoxelSculptVolumeStorageData> AVoxelSculptVolume::GetData() const
{
	return InlineStorage->GetData();
}

TSharedRef<FVoxelRuntimeParameter_SculptVolumeStorage> AVoxelSculptVolume::GetSculptVolumeParameter() const
{
	const TSharedRef<FVoxelRuntimeParameter_SculptVolumeStorage> Parameter = MakeVoxelShared<FVoxelRuntimeParameter_SculptVolumeStorage>();
	Parameter->Data = InlineStorage->GetData();
	Parameter->BakedData = InlineStorage->GetBakedDataData();
	return Parameter;
}

void AVoxelSculptVolume::ClearData()
{
	InlineStorage->GetData()->ClearData();
}