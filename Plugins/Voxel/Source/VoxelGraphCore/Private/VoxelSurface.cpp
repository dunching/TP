// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSurface.h"
#include "FunctionLibrary/VoxelSurfaceFunctionLibrary.h"

FVoxelSurfaceBounds FVoxelSurfaceBounds::Make(const FVoxelBox& Bounds)
{
	FVoxelSurfaceBounds Result;
	Result.bIsEmpty = false;
	Result.Bounds = Bounds;
	Result.Check();
	return Result;
}

FVoxelSurfaceBounds FVoxelSurfaceBounds::Empty()
{
	return {};
}

FVoxelSurfaceBounds FVoxelSurfaceBounds::Infinite()
{
	return Empty().Invert();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelSurfaceBounds::IsEmpty() const
{
	Check();
	return bIsEmpty;
}

bool FVoxelSurfaceBounds::IsInfinite() const
{
	Check();
	return bIsEmpty && bIsInverted;
}

FVoxelBox FVoxelSurfaceBounds::GetBounds() const
{
	Check();

	if (bIsInverted)
	{
		return FVoxelBox::Infinite;
	}
	if (bIsEmpty)
	{
		return FVoxelBox();
	}
	return Bounds;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurfaceBounds FVoxelSurfaceBounds::Invert() const
{
	Check();

	FVoxelSurfaceBounds Result = *this;
	Result.bIsInverted = !Result.bIsInverted;
	Result.Check();
	return Result;
}

FVoxelSurfaceBounds FVoxelSurfaceBounds::TransformBy(const FMatrix& Transform) const
{
	Check();

	if (IsInfinite())
	{
		return Infinite();
	}

	if (IsEmpty())
	{
		ensureVoxelSlow(!bIsInverted);
		return Empty();
	}

	if (bIsInverted)
	{
		FVoxelSurfaceBounds Result = *this;
		Result.Bounds = Result.Bounds.TransformBy(Transform);
		Result.Check();
		return Result;
	}

	FVoxelSurfaceBounds Result = *this;
	Result.Bounds = Result.Bounds.TransformBy(Transform);
	Result.Check();
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurfaceBounds FVoxelSurfaceBounds::Extend(const double Value) const
{
	return Extend(FVector3d(Value));
}

FVoxelSurfaceBounds FVoxelSurfaceBounds::Extend(const FVector3d& Value) const
{
	Check();

	if (IsInfinite())
	{
		return Infinite();
	}

	if (IsEmpty())
	{
		// Might not be accurate?
		// Should be fine as long as Extend is only used for smoothness
		ensureVoxelSlow(!bIsInverted);
		return Empty();
	}

	if (bIsInverted)
	{
		FVoxelSurfaceBounds Result = *this;
		Result.Bounds = Result.Bounds.Extend(-Value);
		Result.Check();
		return Result;
	}

	FVoxelSurfaceBounds Result = *this;
	Result.Bounds = Result.Bounds.Extend(Value);
	Result.Check();
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurfaceBounds FVoxelSurfaceBounds::Scale(const double Value) const
{
	return Scale(FVector3d(Value));
}

FVoxelSurfaceBounds FVoxelSurfaceBounds::Scale(const FVector3d& Value) const
{
	Check();

	if (IsInfinite())
	{
		return Infinite();
	}

	if (IsEmpty())
	{
		ensureVoxelSlow(!bIsInverted);
		return Empty();
	}

	if (bIsInverted)
	{
		FVoxelSurfaceBounds Result = *this;
		Result.Bounds = Result.Bounds.Scale(FVector3d(1.f) / Value);
		Result.Check();
		return Result;
	}

	FVoxelSurfaceBounds Result = *this;
	Result.Bounds = Result.Bounds.Scale(Value);
	Result.Check();
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurfaceBounds FVoxelSurfaceBounds::Union(const FVoxelSurfaceBounds& Other) const
{
	Check();

	if (IsInfinite() ||
		Other.IsInfinite())
	{
		return Infinite();
	}

	if (IsEmpty() &&
		Other.IsEmpty())
	{
		return Empty();
	}

	if (IsEmpty())
	{
		return Other;
	}
	if (Other.IsEmpty())
	{
		return *this;
	}

	ensureVoxelSlow(!IsEmpty());
	ensureVoxelSlow(!Other.IsEmpty());

	if (bIsInverted &&
		Other.bIsInverted)
	{
		if (!Bounds.Intersect(Other.Bounds))
		{
			return Infinite();
		}

		FVoxelSurfaceBounds Result;
		Result.bIsEmpty = false;
		Result.bIsInverted = true;
		Result.Bounds = Bounds.Overlap(Other.Bounds);
		Result.Check();
		return Result;
	}

	if (bIsInverted)
	{
		// Not perfect
		// Return the inverted one which is supposed to be bigger
		return *this;
	}
	if (Other.bIsInverted)
	{
		// Not perfect
		// Return the inverted one which is supposed to be bigger
		return Other;
	}

	ensureVoxelSlow(!bIsInverted);
	ensureVoxelSlow(!Other.bIsInverted);

	FVoxelSurfaceBounds Result;
	Result.bIsEmpty = false;
	Result.Bounds = Bounds.Union(Other.Bounds);
	Result.Check();
	return Result;
}

FVoxelSurfaceBounds FVoxelSurfaceBounds::Intersection(const FVoxelSurfaceBounds& Other) const
{
	return Invert().Union(Other.Invert()).Invert();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int32 FVoxelSurfaceMaterial::Num() const
{
	int32 Num = 1;

	for (const auto& It : Attributes)
	{
		ensure(FVoxelBufferAccessor::MergeNum(Num, It.Value));
	}

	for (const FLayer& Layer : Layers)
	{
		ensure(FVoxelBufferAccessor::MergeNum(Num, Layer.Material));
		ensure(FVoxelBufferAccessor::MergeNum(Num, Layer.Strength));
	}

	return Num;
}

int32 FVoxelSurfaceMaterial::Num_Slow() const
{
	return Num();
}

bool FVoxelSurfaceMaterial::IsValid_Slow() const
{
	int32 Num = 1;

	for (const auto& It : Attributes)
	{
		if (!FVoxelBufferAccessor::MergeNum(Num, It.Value))
		{
			return false;
		}
	}

	for (const FLayer& Layer : Layers)
	{
		if (!FVoxelBufferAccessor::MergeNum(Num, Layer.Material) ||
			!FVoxelBufferAccessor::MergeNum(Num, Layer.Strength))
		{
			return false;
		}
	}

	return true;
}

void FVoxelSurfaceMaterial::GetLayers(
	const FVoxelBufferIterator& Iterator,
	TVoxelArray<ispc::FVoxelSurfaceLayer>& OutLayers) const
{
	OutLayers.Reset(Layers.Num());

	for (const FLayer& Layer : Layers)
	{
		OutLayers.Add(
		{
			Layer.Material.IsConstant(),
			ReinterpretCastPtr<uint16>(Layer.Material.GetData(Iterator)),
			Layer.Strength.IsConstant(),
			Layer.Strength.GetData(Iterator)
		});
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurface FVoxelSurface::CreateLocalSurface(
	const FVoxelGraphNodeRef& NodeRef,
	const FVoxelQuery& Query) const
{
	ensure(!bIsValid);
	ensure(LocalToWorld.IsIdentity());

	FVoxelSurface Result = *this;
	Result.bIsValid = true;
	return MakeVoxelFunctionCaller<UVoxelSurfaceFunctionLibrary>(NodeRef, Query)->ApplyTransform(Result);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurface FVoxelSurface::CreateLocalSurface(
	const UVoxelFunctionLibrary* FunctionLibrary) const
{
	return CreateLocalSurface(FunctionLibrary->GetNodeRef(), FunctionLibrary->GetQuery());
}

TVoxelFutureValue<FVoxelFloatBuffer> FVoxelSurface::GetLocalDistance(const FVoxelQuery& Query) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!ComputeLocalDistance)
	{
		return 1.e6f;
	}

	// Task to flatten callstack
	return
		MakeVoxelTask()
		.Execute<FVoxelFloatBuffer>([ComputeLocalDistance = ComputeLocalDistance, Query]() -> TVoxelFutureValue<FVoxelFloatBuffer>
		{
			const TVoxelFutureValue<FVoxelFloatBuffer> Distance = (*ComputeLocalDistance)(Query);
			if (!Distance.IsValid())
			{
				return 1.e6f;
			}
			return Distance;
		});
}

TVoxelFutureValue<FVoxelSurfaceMaterial> FVoxelSurface::GetLocalMaterial(const FVoxelQuery& Query) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!ComputeMaterial)
	{
		return FVoxelSurfaceMaterial();
	}

	// Task to flatten callstack
	return
		MakeVoxelTask()
		.Execute<FVoxelSurfaceMaterial>([ComputeMaterial = ComputeMaterial, Query]() -> TVoxelFutureValue<FVoxelSurfaceMaterial>
		{
			const TVoxelFutureValue<FVoxelSurfaceMaterial> Material = (*ComputeMaterial)(Query);
			if (!Material.IsValid())
			{
				return FVoxelSurfaceMaterial();
			}
			return Material;
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelFutureValue<FVoxelFloatBuffer> FVoxelSurface::GetDistance(const FVoxelQuery& Query) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!ComputeLocalDistance)
	{
		return 1.e6f;
	}
	if (Query.GetInfo(EVoxelQueryInfo::Query).GetLocalToWorld() != LocalToWorld)
	{
		VOXEL_MESSAGE(Error, "{0}: Surface is missing an ApplyTransform node", Node);
	}

	const TVoxelFutureValue<FVoxelFloatBuffer> Distance = (*ComputeLocalDistance)(Query);
	if (!Distance.IsValid())
	{
		return 1.e6f;
	}

	return Distance;
}

TVoxelFutureValue<FVoxelSurfaceMaterial> FVoxelSurface::GetMaterial(const FVoxelQuery& Query) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!ComputeMaterial)
	{
		return FVoxelSurfaceMaterial();
	}
	if (Query.GetInfo(EVoxelQueryInfo::Query).GetLocalToWorld() != LocalToWorld)
	{
		VOXEL_MESSAGE(Error, "{0}: Surface is missing an ApplyTransform node", Node);
	}

	const TVoxelFutureValue<FVoxelSurfaceMaterial> Material = (*ComputeMaterial)(Query);
	if (!Material.IsValid())
	{
		return FVoxelSurfaceMaterial();
	}

	return Material;
}