// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelChunkSpawner.h"

DEFINE_UNIQUE_VOXEL_ID(FVoxelChunkId);

VOXEL_CONSOLE_VARIABLE(
	VOXELGRAPHCORE_API, int32, GVoxelChunkSpawnerMaxChunks, 1000000,
	"voxel.chunkspawner.MaxChunks",
	"");

VOXEL_CONSOLE_VARIABLE(
	VOXELGRAPHCORE_API, float, GVoxelChunkSpawnerCameraRefreshThreshold, 1.f,
	"voxel.chunkspawner.CameraRefreshThreshold",
	"");

TSharedRef<FVoxelChunkRef> FVoxelChunkSpawnerImpl::CreateChunk(
	const int32 LOD,
	const int32 ChunkSize,
	const FVoxelBox& Bounds) const
{
	if (!ensure(CreateChunkLambda))
	{
		return MakeVoxelShared<FVoxelChunkRef>(FVoxelChunkId(), MakeVoxelShared<FVoxelChunkActionQueue>());
	}

	TSharedPtr<FVoxelChunkRef> ChunkRef;
	CreateChunkLambda(LOD, ChunkSize, Bounds, ChunkRef);

	if (!ensure(ChunkRef))
	{
		return MakeVoxelShared<FVoxelChunkRef>(FVoxelChunkId(), MakeVoxelShared<FVoxelChunkActionQueue>());
	}

	return ChunkRef.ToSharedRef();
}