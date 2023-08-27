// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Point/VoxelPointId.h"
#include "VoxelSpawnableRef.h"
#include "Buffer/VoxelStaticMeshBuffer.h"

class FVoxelAABBTree;

struct VOXELSPAWNER_API FVoxelMeshDataBase
{
public:
	const FVoxelStaticMesh Mesh;
	const FVoxelSpawnableRef SpawnableRef;

	FVoxelMeshDataBase(
		const FVoxelStaticMesh& Mesh,
		const FVoxelSpawnableRef& SpawnableRef)
		: Mesh(Mesh)
		, SpawnableRef(SpawnableRef)
	{
	}

	TVoxelArray<FVoxelPointId> PointIds;
	TVoxelArray<FTransform3f> Transforms;
	FVoxelOptionalBox Bounds;

	int32 NumCustomDatas = 0;
	TVoxelArray<TVoxelArray<float>> CustomDatas_Transient;

	FORCEINLINE int32 Num() const
	{
		checkVoxelSlow(PointIds.Num() == Transforms.Num());
		return PointIds.Num();
	}
	int64 GetAllocatedSize() const;

	TSharedRef<const FVoxelAABBTree> GetTree() const;

private:
	mutable FVoxelFastCriticalSection TreeCriticalSection;
	mutable TSharedPtr<const FVoxelAABBTree> Tree_RequiresLock;
};