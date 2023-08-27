// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "VoxelActorFactories.generated.h"

UCLASS()
class UActorFactoryVoxelSculptVolume : public UActorFactory
{
	GENERATED_BODY()

public:
	UActorFactoryVoxelSculptVolume();

	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
};