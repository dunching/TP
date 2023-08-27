// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "MarchingCube/VoxelMarchingCubeProcessor.h"
#include "TransvoxelData.h"
#include "TransvoxelTransitionData.h"

FVoxelMarchingCubeProcessor::FVoxelMarchingCubeProcessor(
	const int32 ChunkSize,
	const int32 DataSize,
	FVoxelFloatBufferStorage& PackedDistances,
	FVoxelMarchingCubeSurface& Surface)
	: ChunkSize(ChunkSize)
	, DataSize(DataSize)
	, PackedDistances(PackedDistances)
	, Surface(Surface)
{
	VOXEL_FUNCTION_COUNTER();

	ensure(ChunkSize == Surface.ChunkSize);
	ensure(DataSize == ChunkSize + 2);
	ensure(PackedDistances.Num() == FMath::Cube(DataSize));

	const int32 EstimatedNumCells = 4 * ChunkSize * ChunkSize;

	Surface.Cells.Reserve(EstimatedNumCells);
	Surface.Indices.Reserve(12 * EstimatedNumCells);
	Surface.Vertices.Reserve(4 * EstimatedNumCells);
	Surface.CellIndices.Reserve(4 * EstimatedNumCells);

	VertexIndexToCellIndex.Reserve(4 * EstimatedNumCells);
	CacheIndexToVertexIndex.Reserve(EstimatedNumCells);

	// Since we use SignBit below, -0 will lead to different results than +0
	// In practice it looks like a lot of math can converge to -0 (typically, a smooth union very far away from the object)
	// This is an issue because one chunk might get +0, the other -0, and neither of them will generate triangles
	PackedDistances.FixupSignBit();
}

FVoxelMarchingCubeProcessor::~FVoxelMarchingCubeProcessor()
{
	VOXEL_FUNCTION_COUNTER();

	CacheIndexToVertexIndex.Empty();
}

void FVoxelMarchingCubeProcessor::Generate(const bool bGenerateTransitions)
{
	VOXEL_FUNCTION_COUNTER();

	{
		VOXEL_SCOPE_COUNTER("FindCells");

		FindCells<0>();
		FindCells<1>();
		FindCells<2>();
		FindCells<3>();
		FindCells<4>();
		FindCells<5>();
		FindCells<6>();
		FindCells<7>();
	}

	if (Surface.Cells.Num() == 0)
	{
		return;
	}

	if (bGenerateTransitions)
	{
		// Do this before ProcessCells to have all cells
		FindTransitionCells();
	}

	ProcessCells();

	if (bGenerateTransitions)
	{
		ProcessTransitionCells();
	}

	//for (FVector3f& Vertex : Vertices)
	//{
	//	Vertex += (FVector3f(FRandomStream(FMath::Rand()).GetUnitVector()) - 0.5f) / 4.f;
	//}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<int32 Pass>
void FVoxelMarchingCubeProcessor::FindCells()
{
	using namespace Voxel;

	checkVoxelSlow(ChunkSize % 2 == 0);
	const int32 BlockSize = ChunkSize / 2;

	checkVoxelSlow(DataSize % 2 == 0);
	const int32 BlockDataSize = DataSize / 2;

	for (int32 BlockZ = 0; BlockZ < BlockSize; BlockZ++)
	{
		for (int32 BlockY = 0; BlockY < BlockSize; BlockY++)
		{
			const int32 BaseBlockIndex = FVoxelUtilities::Get3DIndex<int32>(BlockDataSize, 0, BlockY, BlockZ);
			for (int32 BlockX = 0; BlockX < BlockSize; BlockX++)
			{
				const int32 IndexOffset = 8 * (BaseBlockIndex + BlockX);

#define INDEX(A, B, C) \
	IndexOffset + \
	8 * ((A + bool(Pass & 1)) / 2) + \
	8 * ((B + bool(Pass & 2)) / 2) * BlockDataSize + \
	8 * ((C + bool(Pass & 4)) / 2) * BlockDataSize * BlockDataSize + \
		((A + bool(Pass & 1)) % 2) + \
		((B + bool(Pass & 2)) % 2) * 2 + \
		((C + bool(Pass & 4)) % 2) * 4

				checkVoxelSlow(INDEX(0, 0, 0) == GetPackedIndex(2 * BlockX + bool(Pass & 1), 2 * BlockY + bool(Pass & 2), 2 * BlockZ + bool(Pass & 4)));
				checkVoxelSlow(INDEX(1, 0, 0) == GetPackedIndex(2 * BlockX + bool(Pass & 1) + 1, 2 * BlockY + bool(Pass & 2), 2 * BlockZ + bool(Pass & 4)));
				checkVoxelSlow(INDEX(0, 1, 0) == GetPackedIndex(2 * BlockX + bool(Pass & 1), 2 * BlockY + bool(Pass & 2) + 1, 2 * BlockZ + bool(Pass & 4)));
				checkVoxelSlow(INDEX(1, 1, 0) == GetPackedIndex(2 * BlockX + bool(Pass & 1) + 1, 2 * BlockY + bool(Pass & 2) + 1, 2 * BlockZ + bool(Pass & 4)));
				checkVoxelSlow(INDEX(0, 0, 1) == GetPackedIndex(2 * BlockX + bool(Pass & 1), 2 * BlockY + bool(Pass & 2), 2 * BlockZ + bool(Pass & 4) + 1));
				checkVoxelSlow(INDEX(1, 0, 1) == GetPackedIndex(2 * BlockX + bool(Pass & 1) + 1, 2 * BlockY + bool(Pass & 2), 2 * BlockZ + bool(Pass & 4) + 1));
				checkVoxelSlow(INDEX(0, 1, 1) == GetPackedIndex(2 * BlockX + bool(Pass & 1), 2 * BlockY + bool(Pass & 2) + 1, 2 * BlockZ + bool(Pass & 4) + 1));
				checkVoxelSlow(INDEX(1, 1, 1) == GetPackedIndex(2 * BlockX + bool(Pass & 1) + 1, 2 * BlockY + bool(Pass & 2) + 1, 2 * BlockZ + bool(Pass & 4) + 1));

				int32 CellCode;
#if (PLATFORM_WINDOWS && !PLATFORM_COMPILER_CLANG) || PLATFORM_ALWAYS_HAS_AVX

				// With 4 loads + movemask, _mm256_setr_ps starts to be faster
				constexpr bool bEnableDoLoad4x = false;

				if constexpr (Pass == 0)
				{
					const __m256 Distance = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset));
					CellCode = _mm256_movemask_ps(Distance);
				}
				else if constexpr (Pass == 1)
				{
					const __m256 DistanceA = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset));
					const __m256 DistanceB = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8));

					const int32 MaskA = _mm256_movemask_ps(DistanceA);
					const int32 MaskB = _mm256_movemask_ps(DistanceB);

					CellCode = ((MaskA & 0b10101010) >> 1) | ((MaskB & 0b01010101) << 1);
				}
				else if constexpr (Pass == 2)
				{
					const __m256 DistanceA = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset));
					const __m256 DistanceB = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8 * BlockDataSize));

					const int32 MaskA = _mm256_movemask_ps(DistanceA);
					const int32 MaskB = _mm256_movemask_ps(DistanceB);

					CellCode = ((MaskA & 0b11001100) >> 2) | ((MaskB & 0b00110011) << 2);
				}
				else if constexpr (Pass == 3 && bEnableDoLoad4x)
				{
					const __m256 DistanceA = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset));
					const __m256 DistanceB = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8));
					const __m256 DistanceC = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8 * BlockDataSize));
					const __m256 DistanceD = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8 + 8 * BlockDataSize));

					const int32 MaskA = _mm256_movemask_ps(DistanceA);
					const int32 MaskB = _mm256_movemask_ps(DistanceB);
					const int32 MaskC = _mm256_movemask_ps(DistanceC);
					const int32 MaskD = _mm256_movemask_ps(DistanceD);

					CellCode =
						((MaskA & 0b10001000) >> 3) |
						((MaskB & 0b01000100) >> 1) |
						((MaskC & 0b00100010) << 1) |
						((MaskD & 0b00010001) << 3);
				}
				else if constexpr (Pass == 4)
				{
					const __m256 DistanceA = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset));
					const __m256 DistanceB = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8 * BlockDataSize * BlockDataSize));

					const int32 MaskA = _mm256_movemask_ps(DistanceA);
					const int32 MaskB = _mm256_movemask_ps(DistanceB);

					CellCode = ((MaskA & 0b11110000) >> 4) | ((MaskB & 0b00001111) << 4);
				}
				else if constexpr (Pass == 5 && bEnableDoLoad4x)
				{
					const __m256 DistanceA = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset));
					const __m256 DistanceB = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8));
					const __m256 DistanceC = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8 * BlockDataSize * BlockDataSize));
					const __m256 DistanceD = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8 + 8 * BlockDataSize * BlockDataSize));

					const int32 MaskA = _mm256_movemask_ps(DistanceA);
					const int32 MaskB = _mm256_movemask_ps(DistanceB);
					const int32 MaskC = _mm256_movemask_ps(DistanceC);
					const int32 MaskD = _mm256_movemask_ps(DistanceD);

					CellCode =
						((MaskA & 0b10100000) >> 5) |
						((MaskB & 0b01010000) >> 3) |
						((MaskC & 0b00001010) << 3) |
						((MaskD & 0b00000101) << 5);
				}
				else if constexpr (Pass == 6 && bEnableDoLoad4x)
				{
					const __m256 DistanceA = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset));
					const __m256 DistanceB = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8 * BlockDataSize));
					const __m256 DistanceC = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8 * BlockDataSize * BlockDataSize));
					const __m256 DistanceD = _mm256_load_ps(&PackedDistances.LoadFast(IndexOffset + 8 * BlockDataSize + 8 * BlockDataSize * BlockDataSize));

					const int32 MaskA = _mm256_movemask_ps(DistanceA);
					const int32 MaskB = _mm256_movemask_ps(DistanceB);
					const int32 MaskC = _mm256_movemask_ps(DistanceC);
					const int32 MaskD = _mm256_movemask_ps(DistanceD);

					CellCode =
						((MaskA & 0b11000000) >> 6) |
						((MaskB & 0b00110000) >> 2) |
						((MaskC & 0b00001100) << 2) |
						((MaskD & 0b00000011) << 6);
				}
				else
				{
					const __m256 Distance = _mm256_setr_ps(
						PackedDistances.LoadFast(INDEX(0, 0, 0)),
						PackedDistances.LoadFast(INDEX(1, 0, 0)),
						PackedDistances.LoadFast(INDEX(0, 1, 0)),
						PackedDistances.LoadFast(INDEX(1, 1, 0)),
						PackedDistances.LoadFast(INDEX(0, 0, 1)),
						PackedDistances.LoadFast(INDEX(1, 0, 1)),
						PackedDistances.LoadFast(INDEX(0, 1, 1)),
						PackedDistances.LoadFast(INDEX(1, 1, 1)));

					CellCode = _mm256_movemask_ps(Distance);
				}
#else
				{
					const float Distance0 = PackedDistances.LoadFast(INDEX(0, 0, 0));
					const float Distance1 = PackedDistances.LoadFast(INDEX(1, 0, 0));
					const float Distance2 = PackedDistances.LoadFast(INDEX(0, 1, 0));
					const float Distance3 = PackedDistances.LoadFast(INDEX(1, 1, 0));
					const float Distance4 = PackedDistances.LoadFast(INDEX(0, 0, 1));
					const float Distance5 = PackedDistances.LoadFast(INDEX(1, 0, 1));
					const float Distance6 = PackedDistances.LoadFast(INDEX(0, 1, 1));
					const float Distance7 = PackedDistances.LoadFast(INDEX(1, 1, 1));

					{
						// Most voxels are going to end up empty
						// We heavily optimize that hot path by just checking if they all have the same sign

						const bool bValue = FVoxelUtilities::SignBit(Distance0);
						if (bValue != FVoxelUtilities::SignBit(Distance1)) { goto Full; }
						if (bValue != FVoxelUtilities::SignBit(Distance2)) { goto Full; }
						if (bValue != FVoxelUtilities::SignBit(Distance3)) { goto Full; }
						if (bValue != FVoxelUtilities::SignBit(Distance4)) { goto Full; }
						if (bValue != FVoxelUtilities::SignBit(Distance5)) { goto Full; }
						if (bValue != FVoxelUtilities::SignBit(Distance6)) { goto Full; }
						if (bValue != FVoxelUtilities::SignBit(Distance7)) { goto Full; }
						continue;
					}

				Full:

					CellCode =
						(FVoxelUtilities::SignBit(Distance0) << 0) |
						(FVoxelUtilities::SignBit(Distance1) << 1) |
						(FVoxelUtilities::SignBit(Distance2) << 2) |
						(FVoxelUtilities::SignBit(Distance3) << 3) |
						(FVoxelUtilities::SignBit(Distance4) << 4) |
						(FVoxelUtilities::SignBit(Distance5) << 5) |
						(FVoxelUtilities::SignBit(Distance6) << 6) |
						(FVoxelUtilities::SignBit(Distance7) << 7);
				}
#endif
#undef INDEX

				if (CellCode == 0 ||
					CellCode == 255)
				{
					continue;
				}

				CellCode = ~CellCode & 0xFF;

				const int32 X = 2 * BlockX + bool(Pass & 1);
				const int32 Y = 2 * BlockY + bool(Pass & 2);
				const int32 Z = 2 * BlockZ + bool(Pass & 4);

				ensureVoxelSlow(FVoxelUtilities::IsValidUINT8(X));
				ensureVoxelSlow(FVoxelUtilities::IsValidUINT8(Y));
				ensureVoxelSlow(FVoxelUtilities::IsValidUINT8(Z));

				FVoxelMarchingCubeCell Cell;
				Cell.X = uint8(X);
				Cell.Y = uint8(Y);
				Cell.Z = uint8(Z);
				Cell.FirstTriangle = CellCode;
				Surface.Cells.Add(Cell);
			}
		}
	}
}

void FVoxelMarchingCubeProcessor::ProcessCells()
{
	VOXEL_FUNCTION_COUNTER();
	using namespace Voxel;

	for (int32 CellIndex = 0; CellIndex < Surface.Cells.Num(); CellIndex++)
	{
		FVoxelMarchingCubeCell& Cell = Surface.Cells[CellIndex];

		const int32 X = Cell.X;
		const int32 Y = Cell.Y;
		const int32 Z = Cell.Z;
		const int32 CellCode = Cell.FirstTriangle;

		checkVoxelSlow(CellCode != 0 && CellCode != 255);

		const int32 CellClass = Transvoxel::GetCellClass(CellCode);
		const Transvoxel::FCellIndices CellIndices = Transvoxel::CellClassToCellIndices[CellClass];
		const Transvoxel::FCellVertices CellVertices = Transvoxel::CellCodeToCellVertices[CellCode];

		// Indices of the vertices used in this cube
		TVoxelStaticArray<int32, 16> CellVertexIndices{ NoInit };
		for (int32 CellVertexIndex = 0; CellVertexIndex < CellVertices.NumVertices(); CellVertexIndex++)
		{
			const Transvoxel::FVertexData VertexData = CellVertices.GetVertexData(CellVertexIndex);

			// A: low point / B: high point
			const int32 IndexA = VertexData.IndexA;
			const int32 IndexB = VertexData.IndexB;
			checkVoxelSlow(0 <= IndexA && IndexA < 8);
			checkVoxelSlow(0 <= IndexB && IndexB < 8);

			const float DistanceA = PackedDistances.LoadFast(GetPackedIndex(X + bool(IndexA & 1), Y + bool(IndexA & 2), Z + bool(IndexA & 4)));
			const float DistanceB = PackedDistances.LoadFast(GetPackedIndex(X + bool(IndexB & 1), Y + bool(IndexB & 2), Z + bool(IndexB & 4)));
			ensureVoxelSlow(FVoxelUtilities::SignBit(DistanceA) != FVoxelUtilities::SignBit(DistanceB));

			const int32 EdgeIndex = VertexData.EdgeIndex;
			checkVoxelSlow(0 <= EdgeIndex && EdgeIndex < 3);

			const FIntVector PositionA(X + bool(IndexA & 1), Y + bool(IndexA & 2), Z + bool(IndexA & 4));
			const FIntVector PositionB(X + bool(IndexB & 1), Y + bool(IndexB & 2), Z + bool(IndexB & 4));

			const int32 CacheIndex = GetCacheIndex(PositionA, EdgeIndex);

			if (const int32* VertexIndex = CacheIndexToVertexIndex.Find(CacheIndex))
			{
				checkVoxelSlow(0 <= *VertexIndex && *VertexIndex < Surface.Vertices.Num());
				CellVertexIndices[CellVertexIndex] = *VertexIndex;
				continue;
			}

			// Compute vertex

			const float Alpha = DistanceA / (DistanceA - DistanceB);

			if (VOXEL_DEBUG)
			{
				FIntVector Offset = FIntVector(ForceInit);
				Offset[EdgeIndex] = 1;
				ensure(PositionA + Offset == PositionB);
			}

			FVector3f Position = FVector3f(PositionA);
			Position[EdgeIndex] += Alpha;

			const int32 VertexIndexA = Surface.Vertices.Add(Position);
			const int32 VertexIndexB = VertexIndexToCellIndex.Add(CellIndex);
			checkVoxelSlow(VertexIndexA == VertexIndexB);

			CacheIndexToVertexIndex.Add_CheckNew(CacheIndex, VertexIndexA);
			CellVertexIndices[CellVertexIndex] = VertexIndexA;
		}

		checkVoxelSlow(Surface.Indices.Num() % 3 == 0);
		const int32 FirstTriangle = Surface.Indices.Num() / 3;

		int32 NumValidTriangles = 0;
		for (int32 Index = 0; Index < CellIndices.NumTriangles(); Index++)
		{
			const int32 VertexIndex0 = CellVertexIndices[CellIndices.GetIndex(3 * Index + 0)];
			const int32 VertexIndex1 = CellVertexIndices[CellIndices.GetIndex(3 * Index + 1)];
			const int32 VertexIndex2 = CellVertexIndices[CellIndices.GetIndex(3 * Index + 2)];

			const FVector3f Vertex0 = Surface.Vertices[VertexIndex0];
			const FVector3f Vertex1 = Surface.Vertices[VertexIndex1];
			const FVector3f Vertex2 = Surface.Vertices[VertexIndex2];

			if (!FVoxelUtilities::IsTriangleValid(Vertex0, Vertex1, Vertex2))
			{
				continue;
			}

			Surface.Indices.Add(VertexIndex0);
			Surface.Indices.Add(VertexIndex1);
			Surface.Indices.Add(VertexIndex2);
			Surface.CellIndices.Add(CellIndex);

			NumValidTriangles++;
		}

		if (NumValidTriangles == 0)
		{
			Surface.Cells.RemoveAtSwap(CellIndex, 1, false);
			CellIndex--;
			continue;
		}

		ensureVoxelSlow(FVoxelUtilities::IsValidUINT8(NumValidTriangles));
		Cell.NumTriangles = uint8(NumValidTriangles);
		Cell.FirstTriangle = FirstTriangle;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMarchingCubeProcessor::FindTransitionCells()
{
	VOXEL_FUNCTION_COUNTER();

	checkVoxelSlow(ChunkSize % 2 == 0);
	const int32 HalfChunkSize = ChunkSize / 2;

	for (int32 Index = 0; Index < 6; Index++)
	{
		TransitionCells[Index].SetNumZeroed(FMath::Square(HalfChunkSize));
	}

	for (const FVoxelMarchingCubeCell& Cell : Surface.Cells)
	{
		const uint32 X = Cell.X;
		const uint32 Y = Cell.Y;
		const uint32 Z = Cell.Z;

		if (X == 0) { TransitionCells[0][FVoxelUtilities::Get2DIndex<int32>(HalfChunkSize, Y / 2, Z / 2)] = true; }
		if (Y == 0) { TransitionCells[2][FVoxelUtilities::Get2DIndex<int32>(HalfChunkSize, Z / 2, X / 2)] = true; }
		if (Z == 0) { TransitionCells[4][FVoxelUtilities::Get2DIndex<int32>(HalfChunkSize, X / 2, Y / 2)] = true; }

		if (X == ChunkSize - 1) { TransitionCells[1][FVoxelUtilities::Get2DIndex<int32>(HalfChunkSize, Z / 2, Y / 2)] = true; }
		if (Y == ChunkSize - 1) { TransitionCells[3][FVoxelUtilities::Get2DIndex<int32>(HalfChunkSize, X / 2, Z / 2)] = true; }
		if (Z == ChunkSize - 1) { TransitionCells[5][FVoxelUtilities::Get2DIndex<int32>(HalfChunkSize, Y / 2, X / 2)] = true; }
	}
}

template<int32 Direction>
void FVoxelMarchingCubeProcessor::ProcessTransitionCells()
{
	using namespace Voxel;

	checkVoxelSlow(ChunkSize % 2 == 0);
	const int32 HalfChunkSize = ChunkSize / 2;

	const int32 NumCells = TransitionCells[Direction].CountSetBits();

	using FTransitionIndex = FVoxelMarchingCubeSurface::FTransitionIndex;

	TVoxelAddOnlyMap<int32, FTransitionIndex> CacheIndexToTransitionIndex;
	CacheIndexToTransitionIndex.Reserve(9 * NumCells);

	Surface.TransitionIndices[Direction].Reserve(20 * NumCells);
	Surface.TransitionVertices[Direction].Reserve(9 * NumCells);
	Surface.TransitionCellIndices[Direction].Reserve(7 * NumCells);

	if (bPerfectTransitions)
	{
		TransitionVerticesToQuery[Direction].Reserve(9 * NumCells);
	}

	TVoxelArray<FTransitionIndex> VertexIndices;
	VertexIndices.Reserve(32);

	TransitionCells[Direction].ForAllSetBits([&](const int32 BitIndex)
	{
		const int32 Y = BitIndex / HalfChunkSize;
		const int32 X = BitIndex - Y * HalfChunkSize;

		TVoxelStaticArray<float, 9> CornerValues{ NoInit };

		CornerValues[0] = PackedDistances.LoadFast(GetPackedIndex<Direction>(2 * X, 2 * Y));
		CornerValues[1] = PackedDistances.LoadFast(GetPackedIndex<Direction>(2 * X + 1, 2 * Y));
		CornerValues[2] = PackedDistances.LoadFast(GetPackedIndex<Direction>(2 * X + 2, 2 * Y));
		CornerValues[3] = PackedDistances.LoadFast(GetPackedIndex<Direction>(2 * X, 2 * Y + 1));
		CornerValues[4] = PackedDistances.LoadFast(GetPackedIndex<Direction>(2 * X + 1, 2 * Y + 1));
		CornerValues[5] = PackedDistances.LoadFast(GetPackedIndex<Direction>(2 * X + 2, 2 * Y + 1));
		CornerValues[6] = PackedDistances.LoadFast(GetPackedIndex<Direction>(2 * X, 2 * Y + 2));
		CornerValues[7] = PackedDistances.LoadFast(GetPackedIndex<Direction>(2 * X + 1, 2 * Y + 2));
		CornerValues[8] = PackedDistances.LoadFast(GetPackedIndex<Direction>(2 * X + 2, 2 * Y + 2));

		int32 CaseCode =
			(FVoxelUtilities::SignBit(CornerValues[0]) << 0) |
			(FVoxelUtilities::SignBit(CornerValues[1]) << 1) |
			(FVoxelUtilities::SignBit(CornerValues[2]) << 2) |
			(FVoxelUtilities::SignBit(CornerValues[5]) << 3) |
			(FVoxelUtilities::SignBit(CornerValues[8]) << 4) |
			(FVoxelUtilities::SignBit(CornerValues[7]) << 5) |
			(FVoxelUtilities::SignBit(CornerValues[6]) << 6) |
			(FVoxelUtilities::SignBit(CornerValues[3]) << 7) |
			(FVoxelUtilities::SignBit(CornerValues[4]) << 8);

		CaseCode = ~CaseCode & 0x1FF;

		if (CaseCode == 0 ||
			CaseCode == 511)
		{
			return;
		}

		checkVoxelSlow(0 <= CaseCode && CaseCode < 512);
		const Transvoxel::Transition::FCellClass CellClass = Transvoxel::Transition::CellCodeToCellClass[CaseCode];
		const Transvoxel::Transition::FVertexDatas& VertexDatas = Transvoxel::Transition::CellCodeToVertexDatas[CaseCode];
		const Transvoxel::Transition::FTransitionCellData& CellData = Transvoxel::Transition::CellClassToTransitionCellData[CellClass.Index];

		VertexIndices.Reset();
		for (int32 VertexDataIndex = 0; VertexDataIndex < CellData.NumVertices; VertexDataIndex++)
		{
			const Transvoxel::Transition::FVertexData VertexData = VertexDatas[VertexDataIndex];

			// A: low point / B: high point
			const int32 VertexIndexA = VertexData.IndexA;
			const int32 VertexIndexB = VertexData.GetIndexB();

			checkVoxelSlow(0 <= VertexIndexA && VertexIndexA < 13);
			checkVoxelSlow(0 <= VertexIndexB && VertexIndexB < 13);

			const auto GetVertexPosition = [&](int32 VertexIndex)
			{
				switch (VertexIndex)
				{
				default: VOXEL_ASSUME(false);
				case 0: return GetPosition<Direction>(2 * X, 2 * Y);
				case 1: return GetPosition<Direction>(2 * X + 1, 2 * Y);
				case 2: return GetPosition<Direction>(2 * X + 2, 2 * Y);
				case 3: return GetPosition<Direction>(2 * X, 2 * Y + 1);
				case 4: return GetPosition<Direction>(2 * X + 1, 2 * Y + 1);
				case 5: return GetPosition<Direction>(2 * X + 2, 2 * Y + 1);
				case 6: return GetPosition<Direction>(2 * X, 2 * Y + 2);
				case 7: return GetPosition<Direction>(2 * X + 1, 2 * Y + 2);
				case 8: return GetPosition<Direction>(2 * X + 2, 2 * Y + 2);

				case 9: return GetPosition<Direction>(2 * X, 2 * Y);
				case 10: return GetPosition<Direction>(2 * X + 2, 2 * Y);
				case 11: return GetPosition<Direction>(2 * X, 2 * Y + 2);
				case 12: return GetPosition<Direction>(2 * X + 2, 2 * Y + 2);
				}
			};
			const auto GetVertexValue = [&](const int32 VertexIndex)
			{
				switch (VertexIndex)
				{
				default: VOXEL_ASSUME(false);
				case 0: return CornerValues[0];
				case 1: return CornerValues[1];
				case 2: return CornerValues[2];
				case 3: return CornerValues[3];
				case 4: return CornerValues[4];
				case 5: return CornerValues[5];
				case 6: return CornerValues[6];
				case 7: return CornerValues[7];
				case 8: return CornerValues[8];

				case 9: return CornerValues[0];
				case 10: return CornerValues[2];
				case 11: return CornerValues[6];
				case 12: return CornerValues[8];
				}
			};

			const FIntVector PositionA = GetVertexPosition(VertexIndexA);
			const FIntVector PositionB = GetVertexPosition(VertexIndexB);

			const bool bAlongX = PositionA.X != PositionB.X;
			const bool bAlongY = PositionA.Y != PositionB.Y;
			const bool bAlongZ = PositionA.Z != PositionB.Z;
			checkVoxelSlow(bAlongX + bAlongY + bAlongZ == 1);

			// Both vertices should be on the same side
			checkVoxelSlow((VertexIndexA < 9) == (VertexIndexB < 9));
			const bool bIsHighRes = VertexIndexA < 9;

			const int32 EdgeIndex = bAlongX ? 0 : bAlongY ? 1 : 2;
			{
				FIntVector NewPositionB = PositionA;
				NewPositionB[EdgeIndex] += bIsHighRes ? 1 : 2;
				checkVoxelSlow(NewPositionB == PositionB);
			}

			const int32 CacheIndex = GetCacheIndex(PositionA, EdgeIndex);

			if (bIsHighRes)
			{
				const int32* VertexIndexPtr = CacheIndexToVertexIndex.Find(CacheIndex);
				if (!ensure(VertexIndexPtr))
				{
					return;
				}

				VerticesToTranslate[*VertexIndexPtr] = true;

				FTransitionIndex TransitionIndex;
				TransitionIndex.bIsRelative = false;
				TransitionIndex.Index = *VertexIndexPtr;
				VertexIndices.Add(TransitionIndex);
				continue;
			}

			if (const FTransitionIndex* TransitionIndex = CacheIndexToTransitionIndex.Find(CacheIndex))
			{
				VertexIndices.Add(*TransitionIndex);
				continue;
			}

			int32 SourceVertex;
			if (const int32* SourceVertexPtr = CacheIndexToVertexIndex.Find(CacheIndex))
			{
				SourceVertex = *SourceVertexPtr;
			}
			else
			{
				FIntVector MiddlePosition = PositionA;
				MiddlePosition[EdgeIndex]++;
				SourceVertex = CacheIndexToVertexIndex.FindChecked(GetCacheIndex(MiddlePosition, EdgeIndex));
			}

			const float ValueA = GetVertexValue(VertexIndexA);
			const float ValueB = GetVertexValue(VertexIndexB);
			ensureVoxelSlow(FVoxelUtilities::SignBit(ValueA) != FVoxelUtilities::SignBit(ValueB));

			const float Alpha = ValueA / (ValueA - ValueB);

			FVector3f Position = FVector3f(PositionA);
			Position[EdgeIndex] += 2 * Alpha;

			const int32 Index = Surface.TransitionVertices[Direction].Add({ Position, SourceVertex });

			if (bPerfectTransitions)
			{
				TransitionVerticesToQuery[Direction].Add(FTransitionVertexToQuery
				{
					Index,
					PositionA,
					PositionB
				});
			}

			FTransitionIndex TransitionIndex;
			TransitionIndex.bIsRelative = true;
			TransitionIndex.Index = Index;

			CacheIndexToTransitionIndex.Add_CheckNew(CacheIndex, TransitionIndex);
			VertexIndices.Add(TransitionIndex);
		}

		const int32 NumIndices = 3 * CellData.NumTriangles;
		for (int32 Index = 0; Index < NumIndices; Index += 3)
		{
			const FTransitionIndex IndexA = VertexIndices[CellData.Indices[Index + 0]];
			const FTransitionIndex IndexB = VertexIndices[CellData.Indices[Index + 1]];
			const FTransitionIndex IndexC = VertexIndices[CellData.Indices[Index + 2]];

			int32 CellIndex = 0;
			if (!IndexA.bIsRelative)
			{
				CellIndex = VertexIndexToCellIndex[IndexA.Index];
			}
			else if (!IndexB.bIsRelative)
			{
				CellIndex = VertexIndexToCellIndex[IndexB.Index];
			}
			else
			{
				ensureVoxelSlow(!IndexC.bIsRelative);
				CellIndex = VertexIndexToCellIndex[IndexC.Index];
			}

			// TODO Split up triangles spanning two cells and find exact cell overlapping triangle
			// Maybe floor/ceil vertices and find cells by position?

			if (CellClass.bIsInverted)
			{
				Surface.TransitionIndices[Direction].Add(IndexA);
				Surface.TransitionIndices[Direction].Add(IndexB);
				Surface.TransitionIndices[Direction].Add(IndexC);
			}
			else
			{
				Surface.TransitionIndices[Direction].Add(IndexC);
				Surface.TransitionIndices[Direction].Add(IndexB);
				Surface.TransitionIndices[Direction].Add(IndexA);
			}

			Surface.TransitionCellIndices[Direction].Add(CellIndex);
		}
	});
}

void FVoxelMarchingCubeProcessor::ProcessTransitionCells()
{
	VOXEL_FUNCTION_COUNTER();

	VerticesToTranslate.SetNumZeroed(Surface.Vertices.Num());

	ProcessTransitionCells<0>();
	ProcessTransitionCells<1>();
	ProcessTransitionCells<2>();
	ProcessTransitionCells<3>();
	ProcessTransitionCells<4>();
	ProcessTransitionCells<5>();

	TVoxelArray<int32> NewToOldIndices;
	FVoxelUtilities::SetNumFast(NewToOldIndices, Surface.Vertices.Num());
	for (int32 Index = 0; Index < Surface.Vertices.Num(); Index++)
	{
		NewToOldIndices[Index] = Index;
	}

	int32 FirstIndex = 0;
	VerticesToTranslate.ForAllSetBits([&](const int32 Index)
	{
		Surface.Vertices.Swap(FirstIndex, Index);
		NewToOldIndices.Swap(FirstIndex, Index);
		FirstIndex++;
	});
	Surface.NumEdgeVertices = FirstIndex;

	TVoxelArray<int32> OldToNewIndices;
	FVoxelUtilities::SetNumFast(OldToNewIndices, Surface.Vertices.Num());
	for (int32 Index = 0; Index < Surface.Vertices.Num(); Index++)
	{
		OldToNewIndices[NewToOldIndices[Index]] = Index;
	}

	for (int32& Index : Surface.Indices)
	{
		Index = OldToNewIndices[Index];
	}

	for (TVoxelArray<FVoxelMarchingCubeSurface::FTransitionIndex>& Array : Surface.TransitionIndices)
	{
		for (FVoxelMarchingCubeSurface::FTransitionIndex& TransitionIndex : Array)
		{
			if (TransitionIndex.bIsRelative)
			{
				continue;
			}

			TransitionIndex.Index = OldToNewIndices[TransitionIndex.Index];
		}
	}
	for (TVoxelArray<FVoxelMarchingCubeSurface::FTransitionVertex>& Array : Surface.TransitionVertices)
	{
		for (FVoxelMarchingCubeSurface::FTransitionVertex& TransitionVertex : Array)
		{
			TransitionVertex.SourceVertex = OldToNewIndices[TransitionVertex.SourceVertex];
		}
	}
}