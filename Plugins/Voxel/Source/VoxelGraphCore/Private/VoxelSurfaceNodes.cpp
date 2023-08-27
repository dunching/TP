// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSurfaceNodes.h"
#include "VoxelBufferUtilities.h"
#include "VoxelPositionQueryParameter.h"
#include "VoxelSurfaceNodesImpl.ispc.generated.h"

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_GetSurfaceDistance, Distance)
{
	FindVoxelQueryParameter(FVoxelPositionQueryParameter, PositionQueryParameter);

	const TValue<FVoxelSurface> Surface = Get(SurfacePin, Query);

	return VOXEL_ON_COMPLETE(PositionQueryParameter, Surface)
	{
		return Surface->GetDistance(Query);
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_GetSurfaceMaterial, Material)
{
	FindVoxelQueryParameter(FVoxelPositionQueryParameter, PositionQueryParameter);

	const TValue<FVoxelSurface> Surface = Get(SurfacePin, Query);

	return VOXEL_ON_COMPLETE(PositionQueryParameter, Surface)
	{
		return Surface->GetMaterial(Query);
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_MakeHeightSurface, Surface)
{
	FVoxelSurface Surface(
		GetNodeRef(),
		FVoxelSurfaceBounds::Infinite());

	Surface.SetDistance(Query, [NodeRef = GetNodeRef(), GetHeight = GetCompute(HeightPin)](const FVoxelQuery& Query)
	{
		const TValue<FVoxelFloatBuffer> FutureHeight = TValue<FVoxelFloatBuffer>(FVoxelBufferUtilities::Query2D(
			ReinterpretCastRef<FVoxelComputeValue>(*GetHeight),
			Query,
			FVoxelPinType::Make<FVoxelFloatBuffer>()));

		return
			MakeVoxelTask(STATIC_FNAME("MakeHeightSurface"))
			.Dependency(FutureHeight)
			.Execute<FVoxelFloatBuffer>([=]() -> TValue<FVoxelFloatBuffer>
			{
				const FVoxelFloatBuffer Height = FutureHeight.Get_CheckCompleted();

				const FVoxelPositionQueryParameter* PositionQueryParameter = Query.GetParameters().Find<FVoxelPositionQueryParameter>();
				if (!PositionQueryParameter)
				{
					RaiseQueryError<FVoxelPositionQueryParameter>(NodeRef);
					return {};
				}

				if (PositionQueryParameter->IsGrid2D())
				{
					VOXEL_MESSAGE(Error, "{0}: MakeHeightSurface requires 3D positions", NodeRef);
					return {};
				}

				const FVoxelFloatBuffer Z = PositionQueryParameter->GetPositions().Z;
				const FVoxelBufferAccessor BufferAccessor(Height, Z);
				if (!BufferAccessor.IsValid())
				{
					RaiseBufferError(NodeRef);
					return {};
				}

				FVoxelFloatBufferStorage Distance;
				Distance.Allocate(BufferAccessor.Num());

				ForeachVoxelBufferChunk(BufferAccessor.Num(), [&](const FVoxelBufferIterator& Iterator)
				{
					ispc::VoxelNode_MakeHeightSurface(
						Height.GetData(Iterator),
						Height.IsConstant(),
						Z.GetData(Iterator),
						Z.IsConstant(),
						Iterator.Num(),
						Distance.GetData(Iterator));
				});

				return FVoxelFloatBuffer::Make(Distance);
			});
	});

	return Surface.CreateLocalSurface(GetNodeRef(), Query);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_MakeVolumetricSurface, Surface)
{
	const TValue<FVoxelBox> Bounds = Get(BoundsPin, Query);

	return VOXEL_ON_COMPLETE(Bounds)
	{
		FVoxelSurface Surface(
			GetNodeRef(),
			// Not a local surface, otherwise can't make it with GetSurfaceDistance
			Query.GetInfo(EVoxelQueryInfo::Query).GetLocalToWorld(),
			Bounds == FVoxelBox() || Bounds.IsInfinite()
			? FVoxelSurfaceBounds::Infinite()
			: FVoxelSurfaceBounds::Make(Bounds));

		Surface.SetDistance(Query, [NodeRef = GetNodeRef(), GetDistance = GetCompute(DistancePin)](const FVoxelQuery& Query)
		{
			return (*GetDistance)(Query);
		});

		return Surface;
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_SetSurfaceMaterial, NewSurface)
{
	const TValue<FVoxelSurface> Surface = Get(SurfacePin, Query);

	return VOXEL_ON_COMPLETE(Surface)
	{
		FVoxelSurface NewSurface = *Surface;
		NewSurface.SetMaterial(Query, [GetMaterial = GetCompute(MaterialPin)](const FVoxelQuery& Query)
		{
			return (*GetMaterial)(Query);
		});
		return NewSurface;
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_MakeSurfaceMaterial, Result)
{
	const TValue<FVoxelMaterialDefinitionBuffer> Material = Get(MaterialPin, Query);

	return VOXEL_ON_COMPLETE(Material)
	{
		FVoxelSurfaceMaterial Result;
		Result.Layers.Add(FVoxelSurfaceMaterial::FLayer
		{
			Material,
			255
		});
		return Result;
	};
}