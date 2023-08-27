// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "Point/VoxelPointNodes.h"
#include "VoxelGraphNodeStatInterface.h"

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_FilterPoints, Out)
{
	const TValue<FVoxelPointSet> Points = Get(InPin, Query);

	return VOXEL_ON_COMPLETE(Points)
	{
		if (Points->Num() == 0)
		{
			return {};
		}

		const TValue<FVoxelBoolBuffer> KeepPoint = Get(KeepPointPin, Points->MakeQuery(Query));

		return VOXEL_ON_COMPLETE(Points, KeepPoint)
		{
			if (!Points->CheckNum(this, KeepPoint.Num()))
			{
				return Points;
			}

			FindVoxelPointSetAttribute(*Points, FVoxelPointAttributes::Id, FVoxelPointIdBuffer, IdBuffer);
			FVoxelNodeStatScope StatScope(*this, Points->Num());

			FVoxelInt32BufferStorage Indices;
			for (int32 Index = 0; Index < Points->Num(); Index++)
			{
				if (KeepPoint[Index])
				{
					Indices.Add(Index);
				}
			}
			return Points->Gather(FVoxelInt32Buffer::Make(Indices));
		};
	};
}

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_DensityFilter, Out)
{
	const TValue<FVoxelPointSet> Points = Get(InPin, Query);

	return VOXEL_ON_COMPLETE(Points)
	{
		if (Points->Num() == 0)
		{
			return {};
		}

		const TValue<FVoxelFloatBuffer> Densities = Get(DensityPin, Points->MakeQuery(Query));
		const TValue<FVoxelSeed> Seed = Get(SeedPin, Points->MakeQuery(Query));

		return VOXEL_ON_COMPLETE(Points, Densities, Seed)
		{
			if (!Points->CheckNum(this, Densities.Num()))
			{
				return Points;
			}

			FindVoxelPointSetAttribute(*Points, FVoxelPointAttributes::Id, FVoxelPointIdBuffer, IdBuffer);
			FVoxelNodeStatScope StatScope(*this, Points->Num());

			const FVoxelPointRandom Random(Seed, STATIC_HASH("DensityFilter"));

			FVoxelInt32BufferStorage Indices;
			for (int32 Index = 0; Index < Points->Num(); Index++)
			{
				const FVoxelPointId Id = IdBuffer[Index];
				const float Fraction = Random.GetFraction(Id);
				if (Fraction >= Densities[Index])
				{
					continue;
				}

				Indices.Add(Index);
			}
			return Points->Gather(FVoxelInt32Buffer::Make(Indices));
		};
	};
}