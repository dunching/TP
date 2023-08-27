// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "FunctionLibrary/VoxelSurfaceFunctionLibrary.h"
#include "FunctionLibrary/VoxelMathFunctionLibrary.h"
#include "VoxelBufferUtilities.h"
#include "VoxelPositionQueryParameter.h"
#include "VoxelSurfaceFunctionLibraryImpl.ispc.generated.h"

FVoxelBox UVoxelSurfaceFunctionLibrary::GetSurfaceBounds(
	const FVoxelSurface& Surface,
	const float InSmoothness,
	const EVoxelTransformSpace TransformSpace) const
{
	FVoxelTransformRef WorldToResult;
	switch (TransformSpace)
	{
	default: ensure(false);
	case EVoxelTransformSpace::Local:
	{
		WorldToResult = GetQuery().GetInfo(EVoxelQueryInfo::Local).GetWorldToLocal();
	}
	break;
	case EVoxelTransformSpace::World:
	{
		WorldToResult = FVoxelTransformRef::Identity();
	}
	break;
	case EVoxelTransformSpace::Query:
	{
		WorldToResult = GetQuery().GetInfo(EVoxelQueryInfo::Query).GetWorldToLocal();
	}
	break;
	}

	const float Smoothness = TransformToSurfaceSpace(Surface, InSmoothness);

	const FVoxelTransformRef Transform = Surface.LocalToWorld * WorldToResult;
	return Surface.LocalBounds
		.Extend(Smoothness)
		.TransformBy(Transform.Get(GetQuery()))
		.GetBounds();
}

float UVoxelSurfaceFunctionLibrary::TransformToSurfaceSpace(
	const FVoxelSurface& Surface,
	const float Value) const
{
	if (!Surface.bIsValid)
	{
		return Value;
	}

	const FVoxelTransformRef LocalToSurface = GetQuery().GetInfo(EVoxelQueryInfo::Local).GetLocalToWorld() * Surface.LocalToWorld.Inverse();
	return Value * LocalToSurface.Get(GetQuery()).GetScaleVector().GetAbsMax();
}

FVoxelSurfaceMaterial UVoxelSurfaceFunctionLibrary::MergeSurfaceMaterials(
	const FVoxelSurfaceMaterial& A,
	const FVoxelSurfaceMaterial& B,
	const FVoxelFloatBuffer& Alpha) const
{
	const int32 Num = ComputeVoxelBuffersNum_Function(A, B, Alpha);

	// TODO
	ensure(A.Attributes.Num() == 0);
	ensure(B.Attributes.Num() == 0);

	const int32 NumLayers = A.Layers.Num() + B.Layers.Num();

	TVoxelArray<FVoxelMaterialDefinitionBufferStorage> Materials;
	TVoxelArray<FVoxelByteBufferStorage> Strengths;
	for (int32 Index = 0; Index < NumLayers; Index++)
	{
		Materials.Emplace_GetRef().Allocate(Num);
		Strengths.Emplace_GetRef().Allocate(Num);
	}

	ForeachVoxelBufferChunk(Num, [&](const FVoxelBufferIterator& Iterator)
	{
		TVoxelArray<ispc::FVoxelSurfaceLayer> LayersA;
		TVoxelArray<ispc::FVoxelSurfaceLayer> LayersB;
		A.GetLayers(Iterator, LayersA);
		B.GetLayers(Iterator, LayersB);

		TVoxelArray<ispc::FVoxelSurfaceWriteLayer> Layers;
		for (int32 Index = 0; Index < NumLayers; Index++)
		{
			Layers.Add(
			{
				ReinterpretCastPtr<uint16>(Materials[Index].GetData(Iterator)),
				Strengths[Index].GetData(Iterator)
			});
		}

		ispc::VoxelSurfaceFunctionLibrary_MergeSurfaces(
			Alpha.GetData(Iterator),
			Alpha.IsConstant(),
			LayersA.GetData(),
			LayersB.GetData(),
			LayersA.Num(),
			LayersB.Num(),
			Iterator.Num(),
			Layers.GetData()
		);
	});

	FVoxelSurfaceMaterial Result;
	for (int32 Index = 0; Index < NumLayers; Index++)
	{
		FVoxelMaterialDefinitionBufferStorage& Material = Materials[Index];
		FVoxelByteBufferStorage& Strength = Strengths[Index];

		Material.TryReduceIntoConstant();

		if (Strength.TryReduceIntoConstant())
		{
			if (Strength.GetConstant() == 0)
			{
				continue;
			}
		}

		Result.Layers.Add(
		{
			FVoxelMaterialDefinitionBuffer::Make(Material),
			FVoxelByteBuffer::Make(Strength)
		});
	}
	return Result;

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurface UVoxelSurfaceFunctionLibrary::ApplyTransform(const FVoxelSurface& Surface) const
{
	if (!Surface.bIsValid)
	{
		return {};
	}

	FVoxelSurface Result(
		GetNodeRef(),
		GetQuery().GetInfo(EVoxelQueryInfo::Query).GetLocalToWorld(),
		Surface.LocalBounds);

	if (Surface.ComputeLocalDistance)
	{
		Result.SetDistance(GetQuery(), [Surface, NodeRef = GetNodeRef()](const FVoxelQuery& Query) -> TVoxelFutureValue<FVoxelFloatBuffer>
		{
			if (!Surface.ComputeLocalDistance)
			{
				return 0.f;
			}

			const FVoxelPositionQueryParameter* PositionQueryParameter = Query.GetParameters().Find<FVoxelPositionQueryParameter>();
			if (!PositionQueryParameter)
			{
				RaiseQueryError<FVoxelPositionQueryParameter>(NodeRef);
				return {};
			}

			const FVoxelVectorBuffer Positions = PositionQueryParameter->GetPositions();

			const FVoxelTransformRef LocalToQuery =
					Query.GetInfo(EVoxelQueryInfo::Local).GetLocalToWorld() *
					Query.GetInfo(EVoxelQueryInfo::Query).GetWorldToLocal();

			const FMatrix Transform = LocalToQuery.Inverse().Get(Query);

			const TSharedRef<FVoxelQueryParameters> Parameters = Query.CloneParameters();
			if (Transform.Equals(FMatrix::Identity))
			{
				// Nothing to be done
			}
			else if (
				PositionQueryParameter->IsGrid3D() &&
				Transform.Rotator().IsNearlyZero() &&
				Transform.GetScaleVector().IsUniform())
			{
				Parameters->Add<FVoxelPositionQueryParameter>().InitializeGrid3D(
					FMatrix44f(Transform).TransformPosition(PositionQueryParameter->GetGrid3D().Start),
					PositionQueryParameter->GetGrid3D().Step * Transform.GetMaximumAxisScale(),
					PositionQueryParameter->GetGrid3D().Size);
			}
			else
			{
				Parameters->Add<FVoxelPositionQueryParameter>().InitializeSparse([=]
				{
					return FVoxelBufferUtilities::ApplyTransform(Positions, Transform);
				});
			}

			const TVoxelFutureValue<FVoxelFloatBuffer> Distance = Surface.GetLocalDistance(Query.MakeNewQuery(Parameters));

			return
				MakeVoxelTask(STATIC_FNAME("TransformDistance"))
				.Dependency(Distance)
				.Execute<FVoxelFloatBuffer>([=]
				{
					return FVoxelBufferUtilities::TransformDistance(Distance.Get_CheckCompleted(), LocalToQuery.Get(Query));
				});
		});
	}

	if (Surface.ComputeMaterial)
	{
		Result.SetMaterial(GetQuery(), [Surface, NodeRef = GetNodeRef()](const FVoxelQuery& Query) -> TVoxelFutureValue<FVoxelSurfaceMaterial>
		{
			if (!Surface.ComputeMaterial)
			{
				return {};
			}

			const FVoxelPositionQueryParameter* PositionQueryParameter = Query.GetParameters().Find<FVoxelPositionQueryParameter>();
			if (!PositionQueryParameter)
			{
				RaiseQueryError<FVoxelPositionQueryParameter>(NodeRef);
				return {};
			}

			const FVoxelVectorBuffer Positions = PositionQueryParameter->GetPositions();

			const FVoxelTransformRef LocalToQuery =
					Query.GetInfo(EVoxelQueryInfo::Local).GetLocalToWorld() *
					Query.GetInfo(EVoxelQueryInfo::Query).GetWorldToLocal();

			const TSharedRef<FVoxelQueryParameters> Parameters = Query.CloneParameters();
			Parameters->Add<FVoxelPositionQueryParameter>().InitializeSparse([=]
			{
				const FMatrix Transform = LocalToQuery.Inverse().Get(Query);
				return FVoxelBufferUtilities::ApplyTransform(Positions, Transform);
			});

			return Surface.GetLocalMaterial(Query.MakeNewQuery(Parameters));
		});
	}

	return Result;
}

FVoxelSurface UVoxelSurfaceFunctionLibrary::Invert(const FVoxelSurface& Surface) const
{
	if (!Surface.bIsValid)
	{
		return {};
	}

	FVoxelSurface Result(
		GetNodeRef(),
		Surface.LocalToWorld,
		Surface.LocalBounds.Invert());

	Result.SetDistance(GetQuery(), [Surface](const FVoxelQuery& Query) -> TVoxelFutureValue<FVoxelFloatBuffer>
	{
		if (!Surface.ComputeLocalDistance)
		{
			return 0.f;
		}

		const TVoxelFutureValue<FVoxelFloatBuffer> FutureDistance = Surface.GetLocalDistance(Query);

		return
			MakeVoxelTask(STATIC_FNAME("Invert"))
			.Dependency(FutureDistance)
			.Execute<FVoxelFloatBuffer>([=]
			{
				const FVoxelFloatBuffer Distance = FutureDistance.Get_CheckCompleted();

				FVoxelFloatBufferStorage NewDistance;
				NewDistance.Allocate(Distance.Num());

				ForeachVoxelBufferChunk(Distance.Num(), [&](const FVoxelBufferIterator& Iterator)
				{
					ispc::VoxelSurfaceFunctionLibrary_Invert(
						Distance.GetData(Iterator),
						NewDistance.GetData(Iterator),
						Iterator.Num());
				});

				return FVoxelFloatBuffer::Make(NewDistance);
			});
	});
	Result.ComputeMaterial = Surface.ComputeMaterial;

	return Result;
}

FVoxelSurface UVoxelSurfaceFunctionLibrary::Grow(
	const FVoxelSurface& Surface,
	const float InAmount) const
{
	if (!Surface.bIsValid)
	{
		return {};
	}

	const float Amount = TransformToSurfaceSpace(Surface, InAmount);

	FVoxelSurface Result(
		GetNodeRef(),
		Surface.LocalToWorld,
		Surface.LocalBounds.Extend(Amount));

	Result.SetDistance(GetQuery(), [Surface, Amount](const FVoxelQuery& Query) -> TVoxelFutureValue<FVoxelFloatBuffer>
	{
		if (!Surface.ComputeLocalDistance)
		{
			return 0.f;
		}

		const TVoxelFutureValue<FVoxelFloatBuffer> FutureDistance = Surface.GetLocalDistance(Query);

		return
			MakeVoxelTask(STATIC_FNAME("Grow"))
			.Dependency(FutureDistance)
			.Execute<FVoxelFloatBuffer>([=]
			{
				const FVoxelFloatBuffer Distance = FutureDistance.Get_CheckCompleted();

				FVoxelFloatBufferStorage NewDistance;
				NewDistance.Allocate(Distance.Num());

				ForeachVoxelBufferChunk(Distance.Num(), [&](const FVoxelBufferIterator& Iterator)
				{
					ispc::VoxelSurfaceFunctionLibrary_Grow(
						Distance.GetData(Iterator),
						NewDistance.GetData(Iterator),
						Amount,
						Iterator.Num());
				});

				return FVoxelFloatBuffer::Make(NewDistance);
			});
	});
	Result.ComputeMaterial = Surface.ComputeMaterial;

	return Result;
}

FVoxelSurface UVoxelSurfaceFunctionLibrary::SmoothUnion(
	const FVoxelSurface& A,
	const FVoxelSurface& B,
	const float InSmoothness) const
{
	if (!A.bIsValid)
	{
		return B;
	}
	if (!B.bIsValid)
	{
		return A;
	}

	if (A.LocalToWorld != B.LocalToWorld)
	{
		VOXEL_MESSAGE(Error, "{0}: A and B have different transforms. Did you forget an ApplyTransform? A: {1} B: {2}", this, A.Node, B.Node);
		return {};
	}

	const float Smoothness = TransformToSurfaceSpace(A, InSmoothness);

	FVoxelSurface Result(
		GetNodeRef(),
		A.LocalToWorld,
		A.LocalBounds.Union(B.LocalBounds).Extend(Smoothness));

	Result.SetDistance(GetQuery(), [A, B, Smoothness, NodeRef = GetNodeRef()](const FVoxelQuery& Query)
	{
		const TVoxelFutureValue<FVoxelFloatBuffer> DistanceA = A.GetLocalDistance(Query);
		const TVoxelFutureValue<FVoxelFloatBuffer> DistanceB = B.GetLocalDistance(Query);

		return
			MakeVoxelTask(STATIC_FNAME("SmoothMin"))
			.Dependencies(DistanceA, DistanceB)
			.Execute<FVoxelFloatBuffer>([=]
			{
				FVoxelFloatBuffer Distance;
				FVoxelFloatBuffer Alpha;
				MakeVoxelFunctionCaller<UVoxelMathFunctionLibrary>(NodeRef, Query)->SmoothMin(
					Distance,
					Alpha,
					DistanceA.Get_CheckCompleted(),
					DistanceB.Get_CheckCompleted(),
					Smoothness);
				return Distance;
			});
	});

	if (A.ComputeMaterial &&
		B.ComputeMaterial)
	{
		Result.SetMaterial(GetQuery(), [A, B, Smoothness, NodeRef = GetNodeRef()](const FVoxelQuery& Query)
		{
			const TVoxelFutureValue<FVoxelFloatBuffer> DistanceA = A.GetLocalDistance(Query);
			const TVoxelFutureValue<FVoxelFloatBuffer> DistanceB = B.GetLocalDistance(Query);

			const TVoxelFutureValue<FVoxelSurfaceMaterial> MaterialA = A.GetLocalMaterial(Query);
			const TVoxelFutureValue<FVoxelSurfaceMaterial> MaterialB = B.GetLocalMaterial(Query);

			return
				MakeVoxelTask(STATIC_FNAME("SmoothMin Material"))
				.Dependencies(DistanceA, DistanceB, MaterialA, MaterialB)
				.Execute<FVoxelSurfaceMaterial>([=]
				{
					FVoxelFloatBuffer Distance;
					FVoxelFloatBuffer Alpha;
					MakeVoxelFunctionCaller<UVoxelMathFunctionLibrary>(NodeRef, Query)->SmoothMin(
						Distance,
						Alpha,
						DistanceA.Get_CheckCompleted(),
						DistanceB.Get_CheckCompleted(),
						Smoothness);

					return MakeVoxelFunctionCaller<UVoxelSurfaceFunctionLibrary>(NodeRef, Query)->MergeSurfaceMaterials(
						MaterialA.Get_CheckCompleted(),
						MaterialB.Get_CheckCompleted(),
						Alpha);
				});
		});
	}
	else
	{
		Result.ComputeMaterial = A.ComputeMaterial ? A.ComputeMaterial : B.ComputeMaterial;
	}

	return Result;
}

FVoxelSurface UVoxelSurfaceFunctionLibrary::SmoothIntersection(
	const FVoxelSurface& A,
	const FVoxelSurface& B,
	const float InSmoothness) const
{
	if (!A.bIsValid ||
		!B.bIsValid)
	{
		return {};
	}

	if (A.LocalToWorld != B.LocalToWorld)
	{
		VOXEL_MESSAGE(Error, "{0}: A and B have different transforms. Did you forget an ApplyTransform?", this);
		return {};
	}

	const float Smoothness = TransformToSurfaceSpace(A, InSmoothness);

	FVoxelSurface Result(
		GetNodeRef(),
		A.LocalToWorld,
		A.LocalBounds.Intersection(B.LocalBounds));

	Result.SetDistance(GetQuery(), [A, B, Smoothness, NodeRef = GetNodeRef()](const FVoxelQuery& Query)
	{
		const TVoxelFutureValue<FVoxelFloatBuffer> DistanceA = A.GetLocalDistance(Query);
		const TVoxelFutureValue<FVoxelFloatBuffer> DistanceB = B.GetLocalDistance(Query);

		return
			MakeVoxelTask(STATIC_FNAME("SmoothMax"))
			.Dependencies(DistanceA, DistanceB)
			.Execute<FVoxelFloatBuffer>([=]
			{
				FVoxelFloatBuffer Distance;
				FVoxelFloatBuffer Alpha;
				MakeVoxelFunctionCaller<UVoxelMathFunctionLibrary>(NodeRef, Query)->SmoothMax(
					Distance,
					Alpha,
					DistanceA.Get_CheckCompleted(),
					DistanceB.Get_CheckCompleted(),
					Smoothness);
				return Distance;
			});
	});

	if (A.ComputeMaterial &&
		B.ComputeMaterial)
	{
		Result.SetMaterial(GetQuery(), [A, B, Smoothness, NodeRef = GetNodeRef()](const FVoxelQuery& Query)
		{
			const TVoxelFutureValue<FVoxelFloatBuffer> DistanceA = A.GetLocalDistance(Query);
			const TVoxelFutureValue<FVoxelFloatBuffer> DistanceB = B.GetLocalDistance(Query);

			const TVoxelFutureValue<FVoxelSurfaceMaterial> MaterialA = A.GetLocalMaterial(Query);
			const TVoxelFutureValue<FVoxelSurfaceMaterial> MaterialB = B.GetLocalMaterial(Query);

			return
				MakeVoxelTask(STATIC_FNAME("SmoothMax Material"))
				.Dependencies(DistanceA, DistanceB, MaterialA, MaterialB)
				.Execute<FVoxelSurfaceMaterial>([=]
				{
					FVoxelFloatBuffer Distance;
					FVoxelFloatBuffer Alpha;
					MakeVoxelFunctionCaller<UVoxelMathFunctionLibrary>(NodeRef, Query)->SmoothMax(
						Distance,
						Alpha,
						DistanceA.Get_CheckCompleted(),
						DistanceB.Get_CheckCompleted(),
						Smoothness);

					return MakeVoxelFunctionCaller<UVoxelSurfaceFunctionLibrary>(NodeRef, Query)->MergeSurfaceMaterials(
						MaterialA.Get_CheckCompleted(),
						MaterialB.Get_CheckCompleted(),
						Alpha);
				});
		});
	}
	else
	{
		Result.ComputeMaterial = A.ComputeMaterial ? A.ComputeMaterial : B.ComputeMaterial;
	}

	return Result;
}

FVoxelSurface UVoxelSurfaceFunctionLibrary::SmoothSubtraction(
	const FVoxelSurface& Surface,
	const FVoxelSurface& SurfaceToSubtract,
	const float Smoothness) const
{
	return SmoothIntersection(Surface, Invert(SurfaceToSubtract), Smoothness);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurface UVoxelSurfaceFunctionLibrary::MakeSphereSurface(
	const FVector& Center,
	const float InRadius) const
{
	const float Radius = FMath::Max(InRadius, 0.f);

	FVoxelSurface Surface(
		GetNodeRef(),
		FVoxelSurfaceBounds::Make(FVoxelBox().Extend(Radius).ShiftBy(Center)));

	Surface.SetDistance(this, [=](const UVoxelSurfaceFunctionLibrary& This)
	{
		return This.MakeSphereSurface_Distance(Center, Radius);
	});

	return Surface.CreateLocalSurface(this);
}

FVoxelSurface UVoxelSurfaceFunctionLibrary::MakeBoxSurface(
	const FVector& Center,
	const FVector& InExtent,
	const float InSmoothness) const
{
	const FVector Extent = FVector::Max(InExtent, FVector::ZeroVector);
	const float Smoothness = FMath::Max(InSmoothness, 0.f);

	FVoxelSurface Surface(
		GetNodeRef(),
		FVoxelSurfaceBounds::Make(FVoxelBox().Extend(Extent + Smoothness).ShiftBy(Center)));

	Surface.SetDistance(this, [=](const UVoxelSurfaceFunctionLibrary& This)
	{
		return This.MakeBoxSurface_Distance(Center, Extent, Smoothness);
	});

	return Surface.CreateLocalSurface(this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFloatBuffer UVoxelSurfaceFunctionLibrary::MakeSphereSurface_Distance(
	const FVector& Center,
	const float Radius) const
{
	VOXEL_FUNCTION_COUNTER();
	FindVoxelQueryParameter_Function(FVoxelPositionQueryParameter, PositionQueryParameter);

	if (PositionQueryParameter->IsGrid2D())
	{
		VOXEL_MESSAGE(Error, "{0}: MakeSphereSurface requires 3D positions", this);
		return {};
	}

	const FVoxelVectorBuffer Positions = PositionQueryParameter->GetPositions();

	FVoxelFloatBufferStorage Distance;
	Distance.Allocate(Positions.Num());

	ForeachVoxelBufferChunk(Positions.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelSurfaceFunctionLibrary_MakeSphereSurface(
			Positions.X.GetData(Iterator),
			Positions.X.IsConstant(),
			Positions.Y.GetData(Iterator),
			Positions.Y.IsConstant(),
			Positions.Z.GetData(Iterator),
			Positions.Z.IsConstant(),
			GetISPCValue(FVector3f(Center)),
			Radius,
			Distance.GetData(Iterator),
			Iterator.Num()
		);
	});

	return FVoxelFloatBuffer::Make(Distance);
}

FVoxelFloatBuffer UVoxelSurfaceFunctionLibrary::MakeBoxSurface_Distance(
	const FVector& Center,
	const FVector& Extent,
	const float Smoothness) const
{
	VOXEL_FUNCTION_COUNTER();
	FindVoxelQueryParameter_Function(FVoxelPositionQueryParameter, PositionQueryParameter);

	if (PositionQueryParameter->IsGrid2D())
	{
		VOXEL_MESSAGE(Error, "{0}: MakeBoxSurface requires 3D positions", this);
		return {};
	}

	const FVoxelVectorBuffer Positions = PositionQueryParameter->GetPositions();

	FVoxelFloatBufferStorage Distance;
	Distance.Allocate(Positions.Num());

	ForeachVoxelBufferChunk(Positions.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelSurfaceFunctionLibrary_MakeBoxSurface(
			Positions.X.GetData(Iterator),
			Positions.X.IsConstant(),
			Positions.Y.GetData(Iterator),
			Positions.Y.IsConstant(),
			Positions.Z.GetData(Iterator),
			Positions.Z.IsConstant(),
			GetISPCValue(FVector3f(Center)),
			GetISPCValue(FVector3f(Extent)),
			Smoothness,
			Distance.GetData(Iterator),
			Iterator.Num()
		);
	});

	return FVoxelFloatBuffer::Make(Distance);
}