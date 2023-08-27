// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SculptVolume/VoxelGetSculptVolumeSurfaceNode.h"
#include "SculptVolume/VoxelSculptVolumeStorage.h"
#include "SculptVolume/VoxelSculptVolumeStorageData.h"
#include "VoxelDependency.h"
#include "VoxelPositionQueryParameter.h"

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_GetSculptVolumeSurface, Surface)
{
	FVoxelSurface Surface(
		GetNodeRef(),
		Query.GetInfo(EVoxelQueryInfo::Query).GetLocalToWorld(),
		FVoxelSurfaceBounds::Infinite());

	Surface.SetDistance(Query, [](const FVoxelQuery& LocalQuery)
	{
		return VOXEL_CALL_NODE(FVoxelNode_GetSculptVolumeSurface_Distance, DistancePin, LocalQuery)
		{
		};
	});
	Surface.SetMaterial(Query, [](const FVoxelQuery& LocalQuery)
	{
		return VOXEL_CALL_NODE(FVoxelNode_GetSculptVolumeSurface_Material, MaterialPin, LocalQuery)
		{
		};
	});

	return Surface;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_GetSculptVolumeSurface_Distance, Distance)
{
	FindVoxelQueryParameter(FVoxelPositionQueryParameter, PositionQueryParameter);

	const TSharedPtr<const FVoxelRuntimeParameter_SculptVolumeStorage> RuntimeParameter = Query.GetInfo(EVoxelQueryInfo::Local).FindParameter<FVoxelRuntimeParameter_SculptVolumeStorage>();
	if (!RuntimeParameter ||
		!ensure(RuntimeParameter->Data))
	{
		// Don't raise error messages, too spammy
		return {};
	}

	static constexpr float InvalidDensity = std::numeric_limits<float>::infinity();

	const FVoxelTransformRef SurfaceToWorld = RuntimeParameter->SurfaceToWorldOverride.IsSet()
		? RuntimeParameter->SurfaceToWorldOverride.GetValue()
		: Query.GetInfo(EVoxelQueryInfo::Local).GetLocalToWorld();

	const FVoxelTransformRef SurfaceToQueryRef =
		SurfaceToWorld *
		Query.GetInfo(EVoxelQueryInfo::Query).GetWorldToLocal();

	const FTransform3f SurfaceToQuery = FTransform3f(FTransform(SurfaceToQueryRef.Get(Query)));

	const float SurfaceToQueryScale = SurfaceToQuery.GetScale3D().GetAbsMax();

	const TSharedRef<FVoxelSculptVolumeStorageData> Data = RuntimeParameter->Data.ToSharedRef();
	const FVoxelBox LocalBounds = PositionQueryParameter->GetBounds().TransformBy(SurfaceToQuery.ToInverseMatrixWithScale());

	Query.GetDependencyTracker().AddDependency(Data->Dependency, LocalBounds);

	TVoxelUniquePtr<FVoxelScopeLock_Read> Lock = MakeVoxelUnique<FVoxelScopeLock_Read>(Data->CriticalSection);

	TSharedPtr<const TVoxelComputeValue<FVoxelSurface>> Compute = Data->Compute_RequiresLock;
	if (!Compute)
	{
		Compute = MakeVoxelShared<TVoxelComputeValue<FVoxelSurface>>(
			[](const FVoxelQuery&) -> TValue<FVoxelSurface>
			{
				return {};
			});
	}

	if (!Data->HasChunks(LocalBounds))
	{
		// Needed for BypassTaskQueue to work
		Lock.Reset();

		const TValue<FVoxelSurface> Surface = (*Compute)(Query);
		return
			MakeVoxelTask()
			.Dependency(Surface)
			.Execute<FVoxelFloatBuffer>([=]
			{
				return Surface.Get_CheckCompleted().GetDistance(Query);
			});
	}

	const FVoxelVectorBuffer Positions = PositionQueryParameter->GetPositions();

	const TSharedRef<TVoxelArray<float>> DensitiesPtr = MakeVoxelShared<TVoxelArray<float>>();
	DensitiesPtr->Reserve(Positions.Num() * 8);

	const TSharedRef<FVoxelBitArray32> ShouldInterpolatePtr = MakeVoxelShared<FVoxelBitArray32>();
	ShouldInterpolatePtr->SetNumZeroed(Positions.Num());

	FVoxelFloatBufferStorage QueryPositionsX;
	FVoxelFloatBufferStorage QueryPositionsY;
	FVoxelFloatBufferStorage QueryPositionsZ;
	{
		VOXEL_SCOPE_COUNTER_FORMAT("Find chunks Num=%d", Positions.Num());

		FIntVector LastChunkKey = FIntVector(MAX_int32);
		const FVoxelSculptVolumeStorageData::FChunk* Chunk = nullptr;

		for (int32 Index = 0; Index < Positions.Num(); Index++)
		{
			const FVector3f Position = SurfaceToQuery.InverseTransformPosition(Positions[Index]);
			if (Position.X < MIN_int32 ||
				Position.Y < MIN_int32 ||
				Position.Z < MIN_int32 ||
				Position.X > MAX_int32 ||
				Position.Y > MAX_int32 ||
				Position.Z > MAX_int32)
			{
				// Can happen when dealing with weird float values, we don't want to crash in the integer logic
				DensitiesPtr->Add(InvalidDensity);
				QueryPositionsX.Add(Positions[Index].X);
				QueryPositionsY.Add(Positions[Index].Y);
				QueryPositionsZ.Add(Positions[Index].Z);
				continue;
			}

			const FIntVector Min = FVoxelUtilities::FloorToInt(Position);
			const FIntVector Max = FVoxelUtilities::CeilToInt(Position);

			struct FHelper
			{
				FORCEINLINE static void GetDensity(
					FIntVector& LastChunkKey,
					const FVoxelSculptVolumeStorageData::FChunk*& Chunk,
					const float SurfaceToQueryScale,
					const FTransform3f& SurfaceToQuery,
					TVoxelArray<float>& Densities,
					const FVoxelSculptVolumeStorageData& Data,
					FVoxelFloatBufferStorage& QueryPositionsX,
					FVoxelFloatBufferStorage& QueryPositionsY,
					FVoxelFloatBufferStorage& QueryPositionsZ,
					const FIntVector& QueryPosition)
				{
					const FIntVector ChunkKey = FVoxelUtilities::DivideFloor_FastLog2(QueryPosition, FVoxelSculptVolumeStorageData::ChunkSizeLog2);
					if (ChunkKey != LastChunkKey)
					{
						LastChunkKey = ChunkKey;
						Chunk = Data.FindChunk(ChunkKey);
					}

					if (!Chunk)
					{
						const FVector3f WorldQueryPosition = SurfaceToQuery.TransformPosition(FVector3f(QueryPosition));

						Densities.Add(InvalidDensity);
						QueryPositionsX.Add(WorldQueryPosition.X);
						QueryPositionsY.Add(WorldQueryPosition.Y);
						QueryPositionsZ.Add(WorldQueryPosition.Z);
						return;
					}

					const FIntVector LocalPosition = QueryPosition - ChunkKey * FVoxelSculptVolumeStorageData::ChunkSize;
					const FVoxelSculptVolumeStorageData::FDensity Density = (*Chunk)[FVoxelUtilities::Get3DIndex<int32>(FVoxelSculptVolumeStorageData::ChunkSize, LocalPosition)];
					ensureVoxelSlow(FVoxelSculptVolumeStorageData::ToDensity(FVoxelSculptVolumeStorageData::FromDensity(Density)) == Density);
					Densities.Add(FVoxelSculptVolumeStorageData::FromDensity(Density) * SurfaceToQueryScale);
				}
			};

			FHelper::GetDensity(
				LastChunkKey,
				Chunk,
				SurfaceToQueryScale,
				SurfaceToQuery,
				*DensitiesPtr,
				*Data,
				QueryPositionsX,
				QueryPositionsY,
				QueryPositionsZ,
				FIntVector(Min.X, Min.Y, Min.Z));

			if (Min == Max)
			{
				continue;
			}

			(*ShouldInterpolatePtr)[Index] = true;

			FHelper::GetDensity(
				LastChunkKey,
				Chunk,
				SurfaceToQueryScale,
				SurfaceToQuery,
				*DensitiesPtr,
				*Data,
				QueryPositionsX,
				QueryPositionsY,
				QueryPositionsZ,
				FIntVector(Max.X, Min.Y, Min.Z));

			FHelper::GetDensity(
				LastChunkKey,
				Chunk,
				SurfaceToQueryScale,
				SurfaceToQuery,
				*DensitiesPtr,
				*Data,
				QueryPositionsX,
				QueryPositionsY,
				QueryPositionsZ,
				FIntVector(Min.X, Max.Y, Min.Z));

			FHelper::GetDensity(
				LastChunkKey,
				Chunk,
				SurfaceToQueryScale,
				SurfaceToQuery,
				*DensitiesPtr,
				*Data,
				QueryPositionsX,
				QueryPositionsY,
				QueryPositionsZ,
				FIntVector(Max.X, Max.Y, Min.Z));

			FHelper::GetDensity(
				LastChunkKey,
				Chunk,
				SurfaceToQueryScale,
				SurfaceToQuery,
				*DensitiesPtr,
				*Data,
				QueryPositionsX,
				QueryPositionsY,
				QueryPositionsZ,
				FIntVector(Min.X, Min.Y, Max.Z));

			FHelper::GetDensity(
				LastChunkKey,
				Chunk,
				SurfaceToQueryScale,
				SurfaceToQuery,
				*DensitiesPtr,
				*Data,
				QueryPositionsX,
				QueryPositionsY,
				QueryPositionsZ,
				FIntVector(Max.X, Min.Y, Max.Z));

			FHelper::GetDensity(
				LastChunkKey,
				Chunk,
				SurfaceToQueryScale,
				SurfaceToQuery,
				*DensitiesPtr,
				*Data,
				QueryPositionsX,
				QueryPositionsY,
				QueryPositionsZ,
				FIntVector(Min.X, Max.Y, Max.Z));

			FHelper::GetDensity(
				LastChunkKey,
				Chunk,
				SurfaceToQueryScale,
				SurfaceToQuery,
				*DensitiesPtr,
				*Data,
				QueryPositionsX,
				QueryPositionsY,
				QueryPositionsZ,
				FIntVector(Max.X, Max.Y, Max.Z));
		}
	}

	FVoxelVectorBuffer QueryPositions;
	QueryPositions.X = FVoxelFloatBuffer::Make(QueryPositionsX);
	QueryPositions.Y = FVoxelFloatBuffer::Make(QueryPositionsY);
	QueryPositions.Z = FVoxelFloatBuffer::Make(QueryPositionsZ);

	TValue<FVoxelFloatBuffer> QueriedDistances;
	if (QueryPositions.Num() > 0)
	{
		const TSharedRef<FVoxelQueryParameters> Parameters = Query.CloneParameters();
		Parameters->Add<FVoxelPositionQueryParameter>().InitializeSparse(QueryPositions);
		const FVoxelQuery NewQuery = Query.MakeNewQuery(Parameters);

		// Needed for BypassTaskQueue to work
		Lock.Reset();

		const TValue<FVoxelSurface> Surface = (*Compute)(NewQuery);
		if (!Surface.IsValid())
		{
			return {};
		}

		QueriedDistances =
			MakeVoxelTask()
			.Dependency(Surface)
			.Execute<FVoxelFloatBuffer>([=]
			{
				return Surface.Get_CheckCompleted().GetDistance(NewQuery);
			});
	}
	else
	{
		QueriedDistances = FVoxelFloatBuffer();
	}

	return VOXEL_ON_COMPLETE(PositionQueryParameter, Positions, DensitiesPtr, ShouldInterpolatePtr, SurfaceToWorld, SurfaceToQuery, QueriedDistances)
	{
		if (QueriedDistances.Num() > 0)
		{
			int32 QueryIndex = 0;
			for (int32 Index = 0; Index < DensitiesPtr->Num(); Index++)
			{
				float& Density = (*DensitiesPtr)[Index];
				if (Density != InvalidDensity)
				{
					continue;
				}

				if (!ensure(
					QueriedDistances.IsConstant() ||
					QueryIndex < QueriedDistances.Num()))
				{
					return {};
				}

				Density = QueriedDistances[QueryIndex++];
			}
			ensure(QueriedDistances.IsConstant() || QueryIndex == QueriedDistances.Num());
		}

		FVoxelFloatBufferStorage FinalDensities;
		FinalDensities.Allocate(Positions.Num());
		{
			VOXEL_SCOPE_COUNTER_FORMAT("TrilinearInterpolation Num=%d", Positions.Num());

			int32 DensityIndex = 0;
			for (int32 Index = 0; Index < Positions.Num(); Index++)
			{
				const FVector3f Position = SurfaceToQuery.InverseTransformPosition(Positions[Index]);
				if (!(*ShouldInterpolatePtr)[Index])
				{
					FinalDensities[Index] = (*DensitiesPtr)[DensityIndex];
					DensityIndex++;
					continue;
				}

				const FVector3f Min = FVoxelUtilities::FloorToFloat(Position);
				const FVector3f Alpha = Position - Min;

				FinalDensities[Index] = FVoxelUtilities::TrilinearInterpolation(
					(*DensitiesPtr)[DensityIndex + 0],
					(*DensitiesPtr)[DensityIndex + 1],
					(*DensitiesPtr)[DensityIndex + 2],
					(*DensitiesPtr)[DensityIndex + 3],
					(*DensitiesPtr)[DensityIndex + 4],
					(*DensitiesPtr)[DensityIndex + 5],
					(*DensitiesPtr)[DensityIndex + 6],
					(*DensitiesPtr)[DensityIndex + 7],
					Alpha.X,
					Alpha.Y,
					Alpha.Z);

				DensityIndex += 8;
			}
			ensure(DensityIndex == DensitiesPtr->Num());
		}

		return FVoxelFloatBuffer::Make(FinalDensities);
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_GetSculptVolumeSurface_Material, Material)
{
	FindVoxelQueryParameter(FVoxelPositionQueryParameter, PositionQueryParameter);

	const TSharedPtr<const FVoxelRuntimeParameter_SculptVolumeStorage> RuntimeParameter = Query.GetInfo(EVoxelQueryInfo::Local).FindParameter<FVoxelRuntimeParameter_SculptVolumeStorage>();
	if (!RuntimeParameter ||
		!ensure(RuntimeParameter->Data))
	{
		// Don't raise error messages, too spammy
		return {};
	}

	const TSharedRef<FVoxelSculptVolumeStorageData> Data = RuntimeParameter->Data.ToSharedRef();

	FVoxelScopeLock_Read Lock(Data->CriticalSection);

	TSharedPtr<const TVoxelComputeValue<FVoxelSurface>> Compute = Data->Compute_RequiresLock;
	if (!Compute)
	{
		Compute = MakeVoxelShared<TVoxelComputeValue<FVoxelSurface>>(
			[](const FVoxelQuery&) -> TValue<FVoxelSurface>
			{
				return {};
			});
	}

	const TValue<FVoxelSurface> Surface = (*Compute)(Query);
	return
		MakeVoxelTask()
		.Dependency(Surface)
		.Execute<FVoxelSurfaceMaterial>([=]
		{
			return Surface.Get_CheckCompleted().GetMaterial(Query);
		});
}