// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelQuery.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "VoxelPositionQueryParameter.generated.h"

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelPositionQueryParameter : public FVoxelQueryParameter
{
	GENERATED_BODY()
	GENERATED_VOXEL_QUERY_PARAMETER_BODY()

public:
	struct FGrid2D
	{
		FVector2f Start = FVector2f::ZeroVector;
		float Step = 0.f;
		FIntPoint Size = FIntPoint::ZeroValue;
	};
	struct FGrid3D
	{
		FVector3f Start = FVector3f::ZeroVector;
		float Step = 0.f;
		FIntVector Size = FIntVector::ZeroValue;
	};
	struct FSparse
	{
		bool bIsGradient = false;
		TOptional<FVoxelBox> Bounds;
		TVoxelUniqueFunction<FVoxelVectorBuffer()> Compute;
	};

	bool IsGrid2D() const
	{
		return Grid2D.IsValid();
	}
	bool IsGrid3D() const
	{
		return Grid3D.IsValid();
	}
	bool IsSparseGradient() const
	{
		return
			Sparse.IsValid() &&
			Sparse->bIsGradient;
	}

	const FGrid2D& GetGrid2D() const
	{
		return *Grid2D;
	}
	const FGrid3D& GetGrid3D() const
	{
		return *Grid3D;
	}

public:
	FVoxelBox GetBounds() const;
	FVoxelVectorBuffer GetPositions() const;

	void InitializeSparse(
		const FVoxelVectorBuffer& Positions,
		const TOptional<FVoxelBox>& Bounds = {});
	void InitializeSparse(
		TVoxelUniqueFunction<FVoxelVectorBuffer()>&& Compute,
		const TOptional<FVoxelBox>& Bounds = {});
	void InitializeSparseGradient(
		const FVoxelVectorBuffer& Positions,
		const TOptional<FVoxelBox>& Bounds = {});

	void InitializeGrid2D(
		const FVector2f& Start,
		float Step,
		const FIntPoint& Size);
	void InitializeGrid3D(
		const FVector3f& Start,
		float Step,
		const FIntVector& Size);

private:
	TSharedPtr<FGrid2D> Grid2D;
	TSharedPtr<FGrid3D> Grid3D;
	TSharedPtr<FSparse> Sparse;

	mutable FVoxelFastCriticalSection CriticalSection;
	mutable TOptional<FVoxelBox> CachedBounds_RequiresLock;
	mutable TOptional<FVoxelVectorBuffer> CachedPositions_RequiresLock;

	void CheckBounds() const;
};