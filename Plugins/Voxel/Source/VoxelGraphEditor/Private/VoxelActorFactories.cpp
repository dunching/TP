// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelActorFactories.h"
#include "VoxelGraphInterface.h"
#include "VoxelParameterContainer.h"
#include "SculptVolume/VoxelSculptVolume.h"

DEFINE_VOXEL_PLACEABLE_ITEM_FACTORY(UActorFactoryVoxelSculptVolume);

UActorFactoryVoxelSculptVolume::UActorFactoryVoxelSculptVolume()
{
	DisplayName = INVTEXT("Voxel Sculpt Volume");
	NewActorClass = AVoxelSculptVolume::StaticClass();
}

void UActorFactoryVoxelSculptVolume::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UVoxelGraphInterface* DefaultGraph = LoadObject<UVoxelGraphInterface>(nullptr, TEXT("/Voxel/Default/SculptVolumeGraph.SculptVolumeGraph"));
	ensure(DefaultGraph);
	CastChecked<AVoxelSculptVolume>(NewActor)->ParameterContainer->Provider = DefaultGraph;
}