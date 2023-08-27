// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelFastOctree.h"

class FVoxelDependency;

class VOXELGRAPHCORE_API FVoxelSculptVolumeStorageBakedData : public TSharedFromThis<FVoxelSculptVolumeStorageBakedData>
{
public:
	static constexpr int32 ChunkSize = 16;
	static constexpr int32 ChunkSizeLog2 = FVoxelUtilities::ExactLog2<ChunkSize>();
	static constexpr int32 MaxDistance = 128;
	static constexpr int32 ChunkCount = FMath::Cube(ChunkSize);

	using FDensity = int16;
	using FChunk = TVoxelStaticArray<FDensity, ChunkCount>;

	FORCEINLINE static FDensity ToDensity(const float Value)
	{
		constexpr int32 Max = TNumericLimits<FDensity>::Max();
		return FMath::Clamp(FMath::RoundToInt(Max * Value / MaxDistance), -Max, Max);
	}
	FORCEINLINE static float FromDensity(const FDensity Value)
	{
		constexpr int32 Max = TNumericLimits<FDensity>::Max();
		return Value / float(Max) * MaxDistance;
	}

public:
	const FName Name;
	const TSharedRef<FVoxelDependency> Dependency;

	struct FOctree : TVoxelFastOctree<>
	{
		FOctree()
			: TVoxelFastOctree(30)
		{
		}
	};
	struct FData
	{
		TSharedRef<FOctree> Octree = MakeVoxelShared<FOctree>();
		TVoxelIntVectorMap<TSharedPtr<FChunk>> Chunks;
		mutable FVoxelSharedCriticalSection CriticalSection;

		FORCEINLINE FChunk* FindChunk(const FIntVector& Key)
		{
			checkVoxelSlow(CriticalSection.IsLocked_Read_Debug());
			const TSharedPtr<FChunk>* Chunk = Chunks.Find(Key);
			if (!Chunk)
			{
				return nullptr;
			}
			return Chunk->Get();
		}
		FORCEINLINE const FChunk* FindChunk(const FIntVector& Key) const
		{
			return ConstCast(this)->FindChunk(Key);
		}

		bool HasChunks(const FVoxelBox& Bounds) const;
		void Serialize(FArchive& Ar);
	};
	FData AdditiveData;
	FData SubtractiveData;

	explicit FVoxelSculptVolumeStorageBakedData(FName Name);

	void Serialize(FArchive& Ar);
};