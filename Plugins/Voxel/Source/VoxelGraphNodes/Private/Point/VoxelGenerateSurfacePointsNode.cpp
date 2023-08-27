// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "Point/VoxelGenerateSurfacePointsNode.h"
#include "VoxelRuntime.h"
#include "TransvoxelData.h"
#include "VoxelGraphNodeStatInterface.h"
#include "VoxelPositionQueryParameter.h"

void FVoxelGenerateSurfacePointsBuilder::Compute()
{
	VOXEL_FUNCTION_COUNTER();
	FVoxelNodeStatScope StatScope(Node, 0);

	// / 4 to allow for distance checks
	//BaseCellSize = MaxResolution / 4; TODO
	BaseCellSize = TargetCellSize / 4;

	const double Size = Bounds.Size().GetMax() / BaseCellSize;
	ensure(Size >= 0);

	const int64 NewDepth = FMath::CeilLogTwo64(FMath::CeilToInt64(Size));
	if (NewDepth > 20)
	{
		VOXEL_MESSAGE(Error, "{0}: bounds too big", Node);
		Finalize();
		return;
	}

	ensure(Depth == -1);
	Depth = NewDepth;

	ensure(Cells.Num() == 0);
	Cells.Add(FIntVector(Bounds.GetCenter() / BaseCellSize));

	ComputeDistances();
}

void FVoxelGenerateSurfacePointsBuilder::ComputeDistances()
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelGenerateSurfacePointsBuilder::ComputeDistances Depth=%d NumCells=%d", Depth, Cells.Num());
	FVoxelNodeStatScope StatScope(Node, 0);

	const int32 CellSize = 1 << Depth;

	FVoxelFloatBufferStorage QueryX; QueryX.Allocate(Cells.Num() * 8);
	FVoxelFloatBufferStorage QueryY; QueryY.Allocate(Cells.Num() * 8);
	FVoxelFloatBufferStorage QueryZ; QueryZ.Allocate(Cells.Num() * 8);

	ensure(CellSize % 4 == 0);
	const int32 QuarterCellSize = CellSize / 4;

	for (int32 Index = 0; Index < Cells.Num(); Index++)
	{
		const FIntVector Cell = Cells[Index];

		QueryX[8 * Index + 0] = (Cell.X - QuarterCellSize) * BaseCellSize;
		QueryX[8 * Index + 1] = (Cell.X + QuarterCellSize) * BaseCellSize;
		QueryX[8 * Index + 2] = (Cell.X - QuarterCellSize) * BaseCellSize;
		QueryX[8 * Index + 3] = (Cell.X + QuarterCellSize) * BaseCellSize;
		QueryX[8 * Index + 4] = (Cell.X - QuarterCellSize) * BaseCellSize;
		QueryX[8 * Index + 5] = (Cell.X + QuarterCellSize) * BaseCellSize;
		QueryX[8 * Index + 6] = (Cell.X - QuarterCellSize) * BaseCellSize;
		QueryX[8 * Index + 7] = (Cell.X + QuarterCellSize) * BaseCellSize;

		QueryY[8 * Index + 0] = (Cell.Y - QuarterCellSize) * BaseCellSize;
		QueryY[8 * Index + 1] = (Cell.Y - QuarterCellSize) * BaseCellSize;
		QueryY[8 * Index + 2] = (Cell.Y + QuarterCellSize) * BaseCellSize;
		QueryY[8 * Index + 3] = (Cell.Y + QuarterCellSize) * BaseCellSize;
		QueryY[8 * Index + 4] = (Cell.Y - QuarterCellSize) * BaseCellSize;
		QueryY[8 * Index + 5] = (Cell.Y - QuarterCellSize) * BaseCellSize;
		QueryY[8 * Index + 6] = (Cell.Y + QuarterCellSize) * BaseCellSize;
		QueryY[8 * Index + 7] = (Cell.Y + QuarterCellSize) * BaseCellSize;

		QueryZ[8 * Index + 0] = (Cell.Z - QuarterCellSize) * BaseCellSize;
		QueryZ[8 * Index + 1] = (Cell.Z - QuarterCellSize) * BaseCellSize;
		QueryZ[8 * Index + 2] = (Cell.Z - QuarterCellSize) * BaseCellSize;
		QueryZ[8 * Index + 3] = (Cell.Z - QuarterCellSize) * BaseCellSize;
		QueryZ[8 * Index + 4] = (Cell.Z + QuarterCellSize) * BaseCellSize;
		QueryZ[8 * Index + 5] = (Cell.Z + QuarterCellSize) * BaseCellSize;
		QueryZ[8 * Index + 6] = (Cell.Z + QuarterCellSize) * BaseCellSize;
		QueryZ[8 * Index + 7] = (Cell.Z + QuarterCellSize) * BaseCellSize;
	}

	const TSharedRef<FVoxelQueryParameters> Parameters = BaseQuery.CloneParameters();
	Parameters->Add<FVoxelGradientStepQueryParameter>().Step = BaseCellSize * CellSize;
	Parameters->Add<FVoxelPositionQueryParameter>().InitializeSparse(FVoxelVectorBuffer::Make(QueryX, QueryY, QueryZ));
	Parameters->Add<FVoxelMinExactDistanceQueryParameter>().MinExactDistance = BaseCellSize * CellSize / 4.f * UE_SQRT_2;

	const TVoxelFutureValue<FVoxelFloatBuffer> Distances = Surface->GetDistance(BaseQuery.MakeNewQuery(Parameters));

	MakeVoxelTask()
	.Dependency(Distances)
	.Execute(MakeWeakPtrLambda(this, [=]
	{
		ProcessDistances(Distances.Get_CheckCompleted().GetStorage());
	}));
}

void FVoxelGenerateSurfacePointsBuilder::ProcessDistances(const FVoxelFloatBufferStorage& Distances)
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelGenerateSurfacePointsBuilder::ProcessDistances Depth=%d NumCells=%d", Depth, Cells.Num());
	FVoxelNodeStatScope StatScope(Node, 0);

	if (Distances.IsConstant() ||
		!ensure(Distances.Num() == 8 * Cells.Num()))
	{
		Finalize();
		return;
	}

	const int32 CellSize = 1 << Depth;
	ensure(CellSize % 4 == 0);
	const int32 HalfCellSize = CellSize / 2;
	const int32 QuarterCellSize = CellSize / 4;
	const float MinDistance = CellSize * BaseCellSize / 4.f * UE_SQRT_2 * (1.f + DistanceChecksTolerance);

	const FVoxelIntBox IntBounds = FVoxelIntBox::FromFloatBox_WithPadding(Bounds / BaseCellSize);

	TVoxelChunkedArray<FIntVector> NewCells;
	{
		VOXEL_SCOPE_COUNTER("Find new cells");

		for (int32 Index = 0; Index < Cells.Num(); Index++)
		{
			if (FMath::Abs(Distances.LoadFast(8 * Index + 0)) < MinDistance) { goto Process; }
			if (FMath::Abs(Distances.LoadFast(8 * Index + 1)) < MinDistance) { goto Process; }
			if (FMath::Abs(Distances.LoadFast(8 * Index + 2)) < MinDistance) { goto Process; }
			if (FMath::Abs(Distances.LoadFast(8 * Index + 3)) < MinDistance) { goto Process; }
			if (FMath::Abs(Distances.LoadFast(8 * Index + 4)) < MinDistance) { goto Process; }
			if (FMath::Abs(Distances.LoadFast(8 * Index + 5)) < MinDistance) { goto Process; }
			if (FMath::Abs(Distances.LoadFast(8 * Index + 6)) < MinDistance) { goto Process; }
			if (FMath::Abs(Distances.LoadFast(8 * Index + 7)) < MinDistance) { goto Process; }
			continue;

		Process:
			const FIntVector Cell = Cells[Index];
			if (!IntBounds.Intersect(FVoxelIntBox(Cell - HalfCellSize, Cell + HalfCellSize)))
			{
				continue;
			}

			NewCells.Add(Cell + FIntVector(-QuarterCellSize, -QuarterCellSize, -QuarterCellSize));
			NewCells.Add(Cell + FIntVector(+QuarterCellSize, -QuarterCellSize, -QuarterCellSize));
			NewCells.Add(Cell + FIntVector(-QuarterCellSize, +QuarterCellSize, -QuarterCellSize));
			NewCells.Add(Cell + FIntVector(+QuarterCellSize, +QuarterCellSize, -QuarterCellSize));
			NewCells.Add(Cell + FIntVector(-QuarterCellSize, -QuarterCellSize, +QuarterCellSize));
			NewCells.Add(Cell + FIntVector(+QuarterCellSize, -QuarterCellSize, +QuarterCellSize));
			NewCells.Add(Cell + FIntVector(-QuarterCellSize, +QuarterCellSize, +QuarterCellSize));
			NewCells.Add(Cell + FIntVector(+QuarterCellSize, +QuarterCellSize, +QuarterCellSize));
		}
	}

	Depth--;
	Cells = MoveTemp(NewCells);

	if (Cells.Num() == 0)
	{
		Finalize();
		return;
	}

	if (Depth > 2)
	{
		ComputeDistances();
		return;
	}

	ComputeFinalDistances();
}

void FVoxelGenerateSurfacePointsBuilder::ComputeFinalDistances()
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelGenerateSurfacePointsBuilder::ComputeFinalDistances Depth=%d NumCells=%d", Depth, Cells.Num());
	FVoxelNodeStatScope StatScope(Node, 0);

	const int32 CellSize = 1 << Depth;
	ensure(CellSize % 2 == 0);
	const int32 HalfCellSize = CellSize / 2;

	int32 NumQueries = 0;
	FVoxelFloatBufferStorage QueryX;
	FVoxelFloatBufferStorage QueryY;
	FVoxelFloatBufferStorage QueryZ;

	TVoxelAddOnlyChunkedMap<FIntVector, int32> KeyToIndex;
	KeyToIndex.Reserve(8 * Cells.Num());

	TVoxelChunkedArray<TVoxelStaticArray<int32, 8>> ValueIndices;
	ValueIndices.SetNumUninitialized(Cells.Num());

	for (int32 CellIndex = 0; CellIndex < Cells.Num(); CellIndex++)
	{
		const FIntVector Cell = Cells[CellIndex];
		TVoxelStaticArray<int32, 8>& CellIndices = ValueIndices[CellIndex];

		for (int32 ChildIndex = 0; ChildIndex < 8; ChildIndex++)
		{
			const FIntVector Key = Cell + FIntVector(
				ChildIndex & 0x1 ? HalfCellSize : -HalfCellSize,
				ChildIndex & 0x2 ? HalfCellSize : -HalfCellSize,
				ChildIndex & 0x4 ? HalfCellSize : -HalfCellSize);
			const uint32 Hash = KeyToIndex.HashValue(Key);

			if (const int32* IndexPtr = KeyToIndex.FindHashed(Hash, Key))
			{
				CellIndices[ChildIndex] = *IndexPtr;
			}
			else
			{
				const int32 Index = NumQueries++;
				QueryX.Add(Key.X * BaseCellSize);
				QueryY.Add(Key.Y * BaseCellSize);
				QueryZ.Add(Key.Z * BaseCellSize);

				KeyToIndex.AddHashed_CheckNew_NoRehash(Hash, Key) = Index;
				CellIndices[ChildIndex] = Index;
			}
		}
	}

	const TSharedRef<FVoxelQueryParameters> Parameters = BaseQuery.CloneParameters();
	Parameters->Add<FVoxelGradientStepQueryParameter>().Step = BaseCellSize * CellSize;
	Parameters->Add<FVoxelPositionQueryParameter>().InitializeSparse(FVoxelVectorBuffer::Make(QueryX, QueryY, QueryZ));

	const TVoxelFutureValue<FVoxelFloatBuffer> SparseDistances = Surface->GetDistance(BaseQuery.MakeNewQuery(Parameters));

	MakeVoxelTask()
	.Dependency(SparseDistances)
	.Execute(MakeWeakPtrLambda(this, [=, ValueIndices = MoveTemp(ValueIndices)]
	{
		ProcessFinalDistances(
			NumQueries,
			ValueIndices,
			SparseDistances.Get_CheckCompleted().GetStorage());
	}));
}

void FVoxelGenerateSurfacePointsBuilder::ProcessFinalDistances(
	const int32 NumQueries,
	const TVoxelChunkedArray<TVoxelStaticArray<int32, 8>>& ValueIndices,
	const FVoxelFloatBufferStorage& SparseDistances)
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelGenerateSurfacePointsBuilder::ProcessFinalDistances Depth=%d NumCells=%d", Depth, Cells.Num());
	FVoxelNodeStatScope StatScope(Node, 0);

	if (SparseDistances.IsConstant() ||
		!ensure(SparseDistances.Num() == NumQueries))
	{
		Finalize();
		return;
	}

	const int32 CellSize = 1 << Depth;
	ensure(CellSize % 2 == 0);
	const int32 HalfCellSize = CellSize / 2;

	for (int32 CellIndex = 0; CellIndex < Cells.Num(); CellIndex++)
	{
		const TVoxelStaticArray<int32, 8>& CellIndices = ValueIndices[CellIndex];
		const float Distance0 = SparseDistances.LoadFast(CellIndices[0]);
		const float Distance1 = SparseDistances.LoadFast(CellIndices[1]);
		const float Distance2 = SparseDistances.LoadFast(CellIndices[2]);
		const float Distance3 = SparseDistances.LoadFast(CellIndices[3]);
		const float Distance4 = SparseDistances.LoadFast(CellIndices[4]);
		const float Distance5 = SparseDistances.LoadFast(CellIndices[5]);
		const float Distance6 = SparseDistances.LoadFast(CellIndices[6]);
		const float Distance7 = SparseDistances.LoadFast(CellIndices[7]);

		if (Distance0 >= 0)
		{
			if (Distance1 >= 0 &&
				Distance2 >= 0 &&
				Distance3 >= 0 &&
				Distance4 >= 0 &&
				Distance5 >= 0 &&
				Distance6 >= 0 &&
				Distance7 >= 0)
			{
				continue;
			}
		}
		else
		{
			if (Distance1 < 0 &&
				Distance2 < 0 &&
				Distance3 < 0 &&
				Distance4 < 0 &&
				Distance5 < 0 &&
				Distance6 < 0 &&
				Distance7 < 0)
			{
				continue;
			}
		}

		int32 CellCode =
			((Distance0 >= 0) << 0) |
			((Distance1 >= 0) << 1) |
			((Distance2 >= 0) << 2) |
			((Distance3 >= 0) << 3) |
			((Distance4 >= 0) << 4) |
			((Distance5 >= 0) << 5) |
			((Distance6 >= 0) << 6) |
			((Distance7 >= 0) << 7);

		CellCode = ~CellCode & 0xFF;

		if (CellCode == 0 ||
			CellCode == 255)
		{
			continue;
		}

		const FIntVector Cell = Cells[CellIndex];
		const FVector3f CellRelativeOffset = FVector3f(Cell - HalfCellSize) / CellSize;

		const Voxel::Transvoxel::FCellVertices CellVertices = Voxel::Transvoxel::CellCodeToCellVertices[CellCode];
		checkVoxelSlow(CellVertices.NumVertices() > 0);

		FVector3f PositionSum = FVector3f(ForceInit);
		for (int32 CellVertexIndex = 0; CellVertexIndex < CellVertices.NumVertices(); CellVertexIndex++)
		{
			const Voxel::Transvoxel::FVertexData VertexData = CellVertices.GetVertexData(CellVertexIndex);

			// A: low point / B: high point
			const int32 IndexA = VertexData.IndexA;
			const int32 IndexB = VertexData.IndexB;
			checkVoxelSlow(0 <= IndexA && IndexA < 8);
			checkVoxelSlow(0 <= IndexB && IndexB < 8);

			const float DistanceA = SparseDistances.LoadFast(CellIndices[IndexA]);
			const float DistanceB = SparseDistances.LoadFast(CellIndices[IndexB]);
			ensureVoxelSlow((DistanceA >= 0) != (DistanceB >= 0));

			const int32 EdgeIndex = VertexData.EdgeIndex;
			checkVoxelSlow(0 <= EdgeIndex && EdgeIndex < 3);

			FVector3f Position = CellRelativeOffset + FVector3f(
				bool(IndexA & 1),
				bool(IndexA & 2),
				bool(IndexA & 4));
			Position[EdgeIndex] += DistanceA / (DistanceA - DistanceB);
			PositionSum += Position;
		}

		PositionSum /= CellVertices.NumVertices();
		const FVector3f Position = PositionSum * CellSize * BaseCellSize;

		// Extend otherwise plane at Z=0 is broken
		if (!Bounds.Extend(KINDA_SMALL_NUMBER).Contains(Position))
		{
			continue;
		}

		const FVector3f Alpha = PositionSum - CellRelativeOffset;
		ensureVoxelSlow(-KINDA_SMALL_NUMBER < Alpha.X && Alpha.X < 1.f + KINDA_SMALL_NUMBER);
		ensureVoxelSlow(-KINDA_SMALL_NUMBER < Alpha.Y && Alpha.Y < 1.f + KINDA_SMALL_NUMBER);
		ensureVoxelSlow(-KINDA_SMALL_NUMBER < Alpha.Z && Alpha.Z < 1.f + KINDA_SMALL_NUMBER);

		const float MinX = FVoxelUtilities::BilinearInterpolation(
			SparseDistances.LoadFast(CellIndices[0b000]),
			SparseDistances.LoadFast(CellIndices[0b010]),
			SparseDistances.LoadFast(CellIndices[0b100]),
			SparseDistances.LoadFast(CellIndices[0b110]),
			Alpha.Y,
			Alpha.Z);

		const float MaxX = FVoxelUtilities::BilinearInterpolation(
			SparseDistances.LoadFast(CellIndices[0b001]),
			SparseDistances.LoadFast(CellIndices[0b011]),
			SparseDistances.LoadFast(CellIndices[0b101]),
			SparseDistances.LoadFast(CellIndices[0b111]),
			Alpha.Y,
			Alpha.Z);

		const float MinY = FVoxelUtilities::BilinearInterpolation(
			SparseDistances.LoadFast(CellIndices[0b000]),
			SparseDistances.LoadFast(CellIndices[0b001]),
			SparseDistances.LoadFast(CellIndices[0b100]),
			SparseDistances.LoadFast(CellIndices[0b101]),
			Alpha.X,
			Alpha.Z);

		const float MaxY = FVoxelUtilities::BilinearInterpolation(
			SparseDistances.LoadFast(CellIndices[0b010]),
			SparseDistances.LoadFast(CellIndices[0b011]),
			SparseDistances.LoadFast(CellIndices[0b110]),
			SparseDistances.LoadFast(CellIndices[0b111]),
			Alpha.X,
			Alpha.Z);

		const float MinZ = FVoxelUtilities::BilinearInterpolation(
			SparseDistances.LoadFast(CellIndices[0b000]),
			SparseDistances.LoadFast(CellIndices[0b001]),
			SparseDistances.LoadFast(CellIndices[0b010]),
			SparseDistances.LoadFast(CellIndices[0b011]),
			Alpha.X,
			Alpha.Y);

		const float MaxZ = FVoxelUtilities::BilinearInterpolation(
			SparseDistances.LoadFast(CellIndices[0b100]),
			SparseDistances.LoadFast(CellIndices[0b101]),
			SparseDistances.LoadFast(CellIndices[0b110]),
			SparseDistances.LoadFast(CellIndices[0b111]),
			Alpha.X,
			Alpha.Y);

		const FVector3f Normal = FVector3f(
			MaxX - MinX,
			MaxY - MinY,
			MaxZ - MinZ).GetSafeNormal();

		Id.Add(FVoxelUtilities::MurmurHashMulti(Cell.X, Cell.Y, Cell.Z, CellSize));

		PositionX.Add(Position.X);
		PositionY.Add(Position.Y);
		PositionZ.Add(Position.Z);

		NormalX.Add(Normal.X);
		NormalY.Add(Normal.Y);
		NormalZ.Add(Normal.Z);
	}

	Finalize();
}

void FVoxelGenerateSurfacePointsBuilder::Finalize()
{
	FVoxelNodeStatScope StatScope(Node, Id.Num());
	Dummy.MarkDummyAsCompleted();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_GenerateSurfacePoints, Out)
{
	const TValue<FVoxelBox> Bounds = Get(BoundsPin, Query);
	const TValue<FVoxelSurface> Surface = Get(SurfacePin, Query);
	const TValue<float> CellSize = Get(CellSizePin, Query);
	const TValue<float> MaxResolution = Get(MaxResolutionPin, Query);
	const TValue<float> DistanceChecksTolerance = Get(DistanceChecksTolerancePin, Query);

	return VOXEL_ON_COMPLETE(Bounds, Surface, CellSize, MaxResolution, DistanceChecksTolerance)
	{
		if (!Bounds.IsValid() ||
			Bounds == FVoxelBox())
		{
			VOXEL_MESSAGE(Error, "{0}: Invalid bounds", this);
			return {};
		}
		if (Bounds.IsInfinite())
		{
			VOXEL_MESSAGE(Error, "{0}: Infinite bounds", this);
			return {};
		}

		const FVoxelDummyFutureValue Dummy = FVoxelFutureValue::MakeDummy();
		const TSharedRef<FVoxelGenerateSurfacePointsBuilder> Builder = MakeVoxelShared<FVoxelGenerateSurfacePointsBuilder>(
			*this,
			Dummy,
			Query,
			Surface,
			FMath::Max(CellSize, 1.f),
			FMath::Max(MaxResolution, 1.f),
			FMath::Max(DistanceChecksTolerance, 0.f),
			Bounds);

		Builder->Compute();

		return VOXEL_ON_COMPLETE(Builder, Dummy)
		{
			const TSharedRef<FVoxelPointSet> PointSet = MakeVoxelShared<FVoxelPointSet>();
			if (Builder->Id.Num() == 0)
			{
				return {};
			}

			PointSet->SetNum(Builder->Id.Num());

			PointSet->Add(FVoxelPointAttributes::Id,
				FVoxelPointIdBuffer::Make(Builder->Id));

			PointSet->Add(FVoxelPointAttributes::Position,
				FVoxelVectorBuffer::Make(
					Builder->PositionX,
					Builder->PositionY,
					Builder->PositionZ));

			PointSet->Add(FVoxelPointAttributes::Normal,
				FVoxelVectorBuffer::Make(
					Builder->NormalX,
					Builder->NormalY,
					Builder->NormalZ));
			return PointSet;
		};
	};
}