// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelPositionQueryParameter.h"
#include "VoxelBufferUtilities.h"

FVoxelBox FVoxelPositionQueryParameter::GetBounds() const
{
	VOXEL_SCOPE_LOCK(CriticalSection);

	if (CachedBounds_RequiresLock)
	{
		return *CachedBounds_RequiresLock;
	}

	if (Grid2D)
	{
		ensure(!Grid3D);
		ensure(!Sparse);

		CachedBounds_RequiresLock = FVoxelBox(
			FVector(FVector2D(Grid2D->Start), FVoxelBox::Infinite.Min.Z),
			FVector(FVector2D(Grid2D->Start) + Grid2D->Step * FVector2D(Grid2D->Size), FVoxelBox::Infinite.Max.Z));
		return *CachedBounds_RequiresLock;
	}
	if (Grid3D)
	{
		ensure(!Grid2D);
		ensure(!Sparse);

		CachedBounds_RequiresLock = FVoxelBox(
			FVector(Grid3D->Start),
			FVector(Grid3D->Start) + Grid3D->Step * FVector(Grid3D->Size));
		return *CachedBounds_RequiresLock;
	}
	if (Sparse)
	{
		ensure(!Grid2D);
		ensure(!Grid3D);

		if (Sparse->Bounds)
		{
			CachedBounds_RequiresLock = Sparse->Bounds.GetValue();
			return *CachedBounds_RequiresLock;
		}

		if (!CachedPositions_RequiresLock)
		{
			VOXEL_SCOPE_COUNTER("Compute sparse positions");
			CachedPositions_RequiresLock = Sparse->Compute();
			ensure(CachedPositions_RequiresLock->Num() > 0);
		}

		VOXEL_SCOPE_COUNTER("Compute sparse bounds");

		const FFloatInterval MinMaxX = CachedPositions_RequiresLock->X.GetStorage().GetMinMaxSafe();
		const FFloatInterval MinMaxY = CachedPositions_RequiresLock->Y.GetStorage().GetMinMaxSafe();
		const FFloatInterval MinMaxZ = CachedPositions_RequiresLock->Z.GetStorage().GetMinMaxSafe();

		CachedBounds_RequiresLock = FVoxelBox(
			FVector(MinMaxX.Min, MinMaxY.Min, MinMaxZ.Min),
			FVector(MinMaxX.Max, MinMaxY.Max, MinMaxZ.Max));

		return *CachedBounds_RequiresLock;
	}

	ensure(false);
	return {};
}

FVoxelVectorBuffer FVoxelPositionQueryParameter::GetPositions() const
{
	VOXEL_SCOPE_LOCK(CriticalSection);

	if (CachedPositions_RequiresLock)
	{
		return *CachedPositions_RequiresLock;
	}

	if (Grid2D)
	{
		ensure(!Grid3D);
		ensure(!Sparse);

		FVoxelFloatBufferStorage WriteX;
		FVoxelFloatBufferStorage WriteY;

		WriteX.Allocate(Grid2D->Size.X * Grid2D->Size.Y);
		WriteY.Allocate(Grid2D->Size.X * Grid2D->Size.Y);

		FVoxelBufferUtilities::WritePositions2D(
			WriteX,
			WriteY,
			Grid2D->Start,
			Grid2D->Step,
			Grid2D->Size);

		CachedPositions_RequiresLock = FVoxelVectorBuffer();
		CachedPositions_RequiresLock->X = FVoxelFloatBuffer::Make(WriteX);
		CachedPositions_RequiresLock->Y = FVoxelFloatBuffer::Make(WriteY);
		CachedPositions_RequiresLock->Z = FVoxelFloatBuffer::Make(0.f);
		return *CachedPositions_RequiresLock;
	}
	if (Grid3D)
	{
		ensure(!Grid2D);
		ensure(!Sparse);

		FVoxelFloatBufferStorage WriteX;
		FVoxelFloatBufferStorage WriteY;
		FVoxelFloatBufferStorage WriteZ;

		WriteX.Allocate(Grid3D->Size.X * Grid3D->Size.Y * Grid3D->Size.Z);
		WriteY.Allocate(Grid3D->Size.X * Grid3D->Size.Y * Grid3D->Size.Z);
		WriteZ.Allocate(Grid3D->Size.X * Grid3D->Size.Y * Grid3D->Size.Z);

		FVoxelBufferUtilities::WritePositions3D(
			WriteX,
			WriteY,
			WriteZ,
			Grid3D->Start,
			Grid3D->Step,
			Grid3D->Size);

		CachedPositions_RequiresLock = FVoxelVectorBuffer::Make(WriteX, WriteY, WriteZ);
		return *CachedPositions_RequiresLock;
	}
	if (Sparse)
	{
		ensure(!Grid2D);
		ensure(!Grid3D);

		VOXEL_SCOPE_COUNTER("Compute sparse positions");
		CachedPositions_RequiresLock = Sparse->Compute();
		ensure(CachedPositions_RequiresLock->Num() > 0);
		return *CachedPositions_RequiresLock;
	}

	ensure(false);
	return {};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelPositionQueryParameter::InitializeSparse(
	const FVoxelVectorBuffer& Positions,
	const TOptional<FVoxelBox>& LocalBounds)
{
	ensure(!Grid2D);
	ensure(!Grid3D);
	ensure(!Sparse);
	ensure(Positions.Num() > 0);

	Sparse = MakeVoxelShared<FSparse>();
	Sparse->Bounds = LocalBounds;
	Sparse->Compute = [Positions]
	{
		return Positions;
	};

	if (VOXEL_DEBUG && LocalBounds)
	{
		CheckBounds();
	}
}

void FVoxelPositionQueryParameter::InitializeSparse(
	TVoxelUniqueFunction<FVoxelVectorBuffer()>&& Compute,
	const TOptional<FVoxelBox>& LocalBounds)
{
	ensure(!Grid2D);
	ensure(!Grid3D);
	ensure(!Sparse);

	Sparse = MakeVoxelShared<FSparse>();
	Sparse->Bounds = LocalBounds;
	Sparse->Compute = MoveTemp(Compute);

	if (VOXEL_DEBUG && LocalBounds)
	{
		CheckBounds();
	}
}

void FVoxelPositionQueryParameter::InitializeSparseGradient(
	const FVoxelVectorBuffer& Positions,
	const TOptional<FVoxelBox>& LocalBounds)
{
	ensure(!Grid2D);
	ensure(!Grid3D);
	ensure(!Sparse);
	ensure(Positions.Num() > 0);

	Sparse = MakeVoxelShared<FSparse>();
	Sparse->bIsGradient = true;
	Sparse->Bounds = LocalBounds;
	Sparse->Compute = [Positions]
	{
		return Positions;
	};

	if (VOXEL_DEBUG && LocalBounds)
	{
		CheckBounds();
	}
}

void FVoxelPositionQueryParameter::InitializeGrid2D(
	const FVector2f& Start,
	const float Step,
	const FIntPoint& Size)
{
	ensure(!Grid2D);
	ensure(!Grid3D);
	ensure(!Sparse);

	Grid2D = MakeVoxelShared<FGrid2D>();
	Grid2D->Start = Start;
	Grid2D->Step = Step;
	Grid2D->Size = Size;

	if (!ensure(Grid2D->Size % 2 == 0))
	{
		Grid2D->Size = 2 * FVoxelUtilities::DivideCeil(Grid2D->Size, 2);
	}
}

void FVoxelPositionQueryParameter::InitializeGrid3D(
	const FVector3f& Start,
	const float Step,
	const FIntVector& Size)
{
	ensure(!Grid2D);
	ensure(!Grid3D);
	ensure(!Sparse);

	Grid3D = MakeVoxelShared<FGrid3D>();
	Grid3D->Start = Start;
	Grid3D->Step = Step;
	Grid3D->Size = Size;

	if (!ensure(Grid3D->Size % 2 == 0))
	{
		Grid3D->Size = 2 * FVoxelUtilities::DivideCeil(Grid3D->Size, 2);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelPositionQueryParameter::CheckBounds() const
{
	VOXEL_FUNCTION_COUNTER();

	const FVoxelVectorBuffer Positions = GetPositions();
	const FVoxelBox Bounds = GetBounds();

	for (int32 Index = 0; Index < Positions.Num(); Index++)
	{
		const FVector3f Position = Positions[Index];

		ensure(Bounds.Min.X - 1 <= Position.X);
		ensure(Bounds.Min.Y - 1 <= Position.Y);
		ensure(Bounds.Min.Z - 1 <= Position.Z);

		ensure(Position.X <= Bounds.Max.X + 1);
		ensure(Position.Y <= Bounds.Max.Y + 1);
		ensure(Position.Z <= Bounds.Max.Z + 1);
	}
}