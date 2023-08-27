// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SculptVolume/VoxelSculptVolumeStorageBakedData.h"
#include "VoxelDependency.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/LargeMemoryReader.h"
#include "Compression/OodleDataCompressionUtil.h"

bool FVoxelSculptVolumeStorageBakedData::FData::HasChunks(const FVoxelBox& Bounds) const
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(CriticalSection.IsLocked_Read_Debug());

	bool bHasChunks = false;
	Octree->TraverseBounds(FVoxelIntBox::FromFloatBox_WithPadding(Bounds / ChunkSize), [&](const FOctree::FNodeRef& NodeRef)
	{
		if (NodeRef.GetHeight() == 0)
		{
			bHasChunks = true;
		}
		return !bHasChunks;
	});
	return bHasChunks;
}

void FVoxelSculptVolumeStorageBakedData::FData::Serialize(FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER();

	using FVersion = DECLARE_VOXEL_VERSION
	(
		FirstVersion
	);

	int32 Version = FVersion::LatestVersion;
	Ar << Version;
	check(Version == FVersion::FirstVersion);

	if (Ar.IsSaving())
	{
		FLargeMemoryWriter Writer;
		{
			FVoxelScopeLock_Read Lock(CriticalSection);

			int32 DensitySize = sizeof(FDensity);
			Writer << DensitySize;

			TArray<FIntVector> Keys;
			Chunks.GenerateKeyArray(Keys);
			Writer << Keys;

			for (const FIntVector& Key : Keys)
			{
				Writer << *Chunks[Key];
			}
		}

		TArray64<uint8> CompressedData;
		{
			VOXEL_SCOPE_COUNTER("Compress");
			ensure(FOodleCompressedArray::CompressData64(
				CompressedData,
				Writer.GetData(),
				Writer.TotalSize(),
				FOodleDataCompression::ECompressor::Kraken,
				FOodleDataCompression::ECompressionLevel::Normal));
		}

		CompressedData.BulkSerialize(Ar);
	}
	else
	{
		check(Ar.IsLoading());
		ensure(Chunks.Num() == 0);

		TArray64<uint8> CompressedData;
		CompressedData.BulkSerialize(Ar);

		TArray64<uint8> Data;
		{
			VOXEL_SCOPE_COUNTER("Decompress");
			if (!ensure(FOodleCompressedArray::DecompressToTArray64(Data, CompressedData)))
			{
				return;
			}
		}

		FLargeMemoryReader Reader(Data.GetData(), Data.Num());

		FVoxelScopeLock_Write Lock(CriticalSection);

		Octree = MakeVoxelShared<FOctree>();
		Chunks.Reset();

		int32 DensitySize = 0;
		Reader << DensitySize;

		if (!ensure(DensitySize == sizeof(FDensity)))
		{
			return;
		}

		TArray<FIntVector> Keys;
		Reader << Keys;

		for (const FIntVector& Key : Keys)
		{
			const TSharedRef<FChunk> Chunk = MakeVoxelShared<FChunk>(ForceInit);
			Reader << *Chunk;
			Chunks.Add(Key, Chunk);

			Octree->TraverseBounds(FVoxelIntBox(Key), [&](const FOctree::FNodeRef& NodeRef)
			{
				if (NodeRef.GetHeight() > 0)
				{
					Octree->CreateAllChildren(NodeRef);
				}
			});
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSculptVolumeStorageBakedData::FVoxelSculptVolumeStorageBakedData(const FName Name)
	: Name(Name)
	, Dependency(FVoxelDependency::Create(STATIC_FNAME("DensityCanvas"), Name))
{
}

void FVoxelSculptVolumeStorageBakedData::Serialize(FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER();

	AdditiveData.Serialize(Ar);
	SubtractiveData.Serialize(Ar);
}