// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelTransformRef.h"
#include "VoxelFunctionLibrary.h"
#include "Buffer/VoxelBaseBuffers.h"
#include "Material/VoxelMaterialDefinition.h"
#include "VoxelSurface.generated.h"

namespace ispc
{
	struct FVoxelSurfaceLayer
	{
		bool bConstantMaterials = false;
		const uint16* Materials = nullptr;
		bool bConstantStrengths = false;
		const uint8* Strengths = nullptr;
	};
}
#define __ISPC_STRUCT_FVoxelSurfaceLayer__

class VOXELGRAPHCORE_API FVoxelSurfaceBounds
{
public:
	FVoxelSurfaceBounds() = default;

	static FVoxelSurfaceBounds Make(const FVoxelBox& Bounds);
	static FVoxelSurfaceBounds Empty();
	static FVoxelSurfaceBounds Infinite();

	bool IsEmpty() const;
	bool IsInfinite() const;
	FVoxelBox GetBounds() const;

	FVoxelSurfaceBounds Invert() const;
	FVoxelSurfaceBounds TransformBy(const FMatrix& Transform) const;

	FVoxelSurfaceBounds Extend(double Value) const;
	FVoxelSurfaceBounds Extend(const FVector3d& Value) const;

	FVoxelSurfaceBounds Scale(double Value) const;
	FVoxelSurfaceBounds Scale(const FVector3d& Value) const;

	FVoxelSurfaceBounds Union(const FVoxelSurfaceBounds& Other) const;
	FVoxelSurfaceBounds Intersection(const FVoxelSurfaceBounds& Other) const;

private:
	bool bIsEmpty = true;
	bool bIsInverted = false;
	FVoxelBox Bounds;

	FORCEINLINE void Check() const
	{
		ensureVoxelSlow(!bIsEmpty || Bounds == FVoxelBox());
		ensureVoxelSlow(bIsEmpty || Bounds != FVoxelBox());
		ensureVoxelSlow(!Bounds.IsInfinite());
		ensureVoxelSlow(Bounds.Min.X <= Bounds.Max.X);
		ensureVoxelSlow(Bounds.Min.Y <= Bounds.Max.Y);
		ensureVoxelSlow(Bounds.Min.Z <= Bounds.Max.Z);
	}
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelSurfaceMaterial : public FVoxelBufferInterface
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	TVoxelMap<FName, FVoxelFloatBuffer> Attributes;

	struct FLayer
	{
		FVoxelMaterialDefinitionBuffer Material;
		FVoxelByteBuffer Strength;
	};
	TVoxelArray<FLayer> Layers;

	int32 Num() const;
	virtual int32 Num_Slow() const final override;
	virtual bool IsValid_Slow() const final override;

	void GetLayers(
		const FVoxelBufferIterator& Iterator,
		TVoxelArray<ispc::FVoxelSurfaceLayer>& OutLayers) const;
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelSurface
{
	GENERATED_BODY()

public:
	bool bIsValid = false;
	FVoxelGraphNodeRef Node;
	FVoxelTransformRef LocalToWorld;
	FVoxelSurfaceBounds LocalBounds;

	TSharedPtr<const TVoxelComputeValue<FVoxelFloatBuffer>> ComputeLocalDistance;
	TSharedPtr<const TVoxelComputeValue<FVoxelSurfaceMaterial>> ComputeMaterial;

public:
	FVoxelSurface() = default;
	FVoxelSurface(
		const FVoxelGraphNodeRef& Node,
		const FVoxelTransformRef& LocalToWorld,
		const FVoxelSurfaceBounds& LocalBounds)
		: bIsValid(true)
		, Node(Node)
		, LocalToWorld(LocalToWorld)
		, LocalBounds(LocalBounds)
	{
	}
	// Local surface
	FVoxelSurface(
		const FVoxelGraphNodeRef& Node,
		const FVoxelSurfaceBounds& LocalBounds)
		: Node(Node)
		, LocalBounds(LocalBounds)
	{
	}

	FVoxelSurface CreateLocalSurface(
		const FVoxelGraphNodeRef& NodeRef,
		const FVoxelQuery& Query) const;
	FVoxelSurface CreateLocalSurface(
		const UVoxelFunctionLibrary* FunctionLibrary) const;

public:
	TVoxelFutureValue<FVoxelFloatBuffer> GetLocalDistance(const FVoxelQuery& Query) const;
	TVoxelFutureValue<FVoxelSurfaceMaterial> GetLocalMaterial(const FVoxelQuery& Query) const;

	TVoxelFutureValue<FVoxelFloatBuffer> GetDistance(const FVoxelQuery& Query) const;
	TVoxelFutureValue<FVoxelSurfaceMaterial> GetMaterial(const FVoxelQuery& Query) const;

public:
	template<typename LambdaType>
	void SetDistance(const FVoxelQuery& Query, LambdaType&& Lambda)
	{
		ensure(!ComputeLocalDistance);
		ComputeLocalDistance = MakeVoxelShared<TVoxelComputeValue<FVoxelFloatBuffer>>([Lambda = MoveTemp(Lambda), Context = Query.GetSharedContext()](const FVoxelQuery& InQuery)
		{
			const FVoxelQuery NewQuery = InQuery.MakeNewQuery(Context);
			const FVoxelQueryScope Scope(NewQuery);
			return Lambda(NewQuery);
		});
	}
	template<typename FunctionLibraryType, typename LambdaType>
	void SetDistance(const FunctionLibraryType* FunctionLibrary, LambdaType&& Lambda)
	{
		this->SetDistance(FunctionLibrary->GetQuery(), [NodeRef = FunctionLibrary->GetNodeRef(), Lambda = MoveTemp(Lambda)](const FVoxelQuery& Query)
		{
			return Lambda(*MakeVoxelFunctionCaller<FunctionLibraryType>(NodeRef, Query));
		});
	}

	template<typename LambdaType>
	void SetMaterial(const FVoxelQuery& Query, LambdaType&& Lambda)
	{
		ensure(!ComputeMaterial);
		ComputeMaterial = MakeVoxelShared<TVoxelComputeValue<FVoxelSurfaceMaterial>>([Lambda = MoveTemp(Lambda), Context = Query.GetSharedContext()](const FVoxelQuery& InQuery)
		{
			const FVoxelQuery NewQuery = InQuery.MakeNewQuery(Context);
			const FVoxelQueryScope Scope(NewQuery);
			return Lambda(NewQuery);
		});
	}
	template<typename FunctionLibraryType, typename LambdaType>
	void SetMaterial(const FunctionLibraryType* FunctionLibrary, LambdaType&& Lambda)
	{
		this->SetMaterial(FunctionLibrary->GetQuery(), [NodeRef = FunctionLibrary->GetNodeRef(), Lambda = MoveTemp(Lambda)](const FVoxelQuery& Query)
		{
			return Lambda(*MakeVoxelFunctionCaller<FunctionLibraryType>(NodeRef, Query));
		});
	}
};