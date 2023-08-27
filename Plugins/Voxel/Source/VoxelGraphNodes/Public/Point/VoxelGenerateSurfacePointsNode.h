// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelSurface.h"
#include "Point/VoxelPointSet.h"
#include "VoxelGenerateSurfacePointsNode.generated.h"

struct FVoxelNode_GenerateSurfacePoints;

class FVoxelGenerateSurfacePointsBuilder : public TSharedFromThis<FVoxelGenerateSurfacePointsBuilder>
{
public:
	const FVoxelNode_GenerateSurfacePoints& Node;
	const FVoxelDummyFutureValue Dummy;
	const FVoxelQuery BaseQuery;
	const TSharedRef<const FVoxelSurface> Surface;
	const float TargetCellSize;
	const float MaxResolution;
	const float DistanceChecksTolerance;
	const FVoxelBox Bounds;

	float BaseCellSize = 0.f;
	int32 Depth = -1;
	TVoxelChunkedArray<FIntVector> Cells;

	FVoxelPointIdBufferStorage Id;
	FVoxelFloatBufferStorage PositionX;
	FVoxelFloatBufferStorage PositionY;
	FVoxelFloatBufferStorage PositionZ;
	FVoxelFloatBufferStorage NormalX;
	FVoxelFloatBufferStorage NormalY;
	FVoxelFloatBufferStorage NormalZ;

	FVoxelGenerateSurfacePointsBuilder(
		const FVoxelNode_GenerateSurfacePoints& Node,
		const FVoxelDummyFutureValue& Dummy,
		const FVoxelQuery& BaseQuery,
		const TSharedRef<const FVoxelSurface>& Surface,
		const float TargetCellSize,
		const float MaxResolution,
		const float DistanceChecksTolerance,
		const FVoxelBox& Bounds)
		: Node(Node)
		, Dummy(Dummy)
		, BaseQuery(BaseQuery)
		, Surface(Surface)
		, TargetCellSize(TargetCellSize)
		, MaxResolution(MaxResolution)
		, DistanceChecksTolerance(DistanceChecksTolerance)
		, Bounds(Bounds)
	{
	}

	void Compute();

private:
	void ComputeDistances();
	void ProcessDistances(const FVoxelFloatBufferStorage& Distances);
	void ComputeFinalDistances();
	void ProcessFinalDistances(
		int32 NumQueries,
		const TVoxelChunkedArray<TVoxelStaticArray<int32, 8>>& ValueIndices,
		const FVoxelFloatBufferStorage& SparseDistances);
	void Finalize();
};

// Generate points on a voxel surface
USTRUCT(Category = "Point")
struct VOXELGRAPHNODES_API FVoxelNode_GenerateSurfacePoints : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelBox, Bounds, nullptr);
	VOXEL_INPUT_PIN(FVoxelSurface, Surface, nullptr);
	// A point will be placed in every cell the surface intersects with
	// This is more or less the average distance between points
	VOXEL_INPUT_PIN(float, CellSize, 100.f);
	// Max surface resolution to check for
	// Decrease if you're missing points due to the surface being too "thin"
	VOXEL_INPUT_PIN(float, MaxResolution, 100.f, AdvancedDisplay);
	// Keep low, increase if you have missing points
	VOXEL_INPUT_PIN(float, DistanceChecksTolerance, 1.f, AdvancedDisplay);
	VOXEL_OUTPUT_PIN(FVoxelPointSet, Out);
};