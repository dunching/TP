// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

class FViewport;

struct VOXELCORE_API FVoxelGameUtilities
{
	static FViewport* GetViewport(const UWorld* World);

	static bool GetCameraView(const UWorld* World, FVector& OutPosition, FRotator& OutRotation, float& OutFOV);
	static bool GetCameraView(const UWorld* World, FVector& OutPosition)
	{
		FRotator Rotation;
		float FOV;
		return GetCameraView(World, OutPosition, Rotation, FOV);
	}

#if WITH_EDITOR
	static bool IsActorSelected_AnyThread(FObjectKey Actor);
#endif

	static void CopyBodyInstance(FBodyInstance& Dest, const FBodyInstance& Source);
};