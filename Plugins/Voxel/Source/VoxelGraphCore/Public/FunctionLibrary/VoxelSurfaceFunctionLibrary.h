// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelSurface.h"
#include "VoxelFunctionLibrary.h"
#include "VoxelSurfaceFunctionLibrary.generated.h"

UENUM()
enum class EVoxelTransformSpace : uint8
{
	// Transform space of the actor of this graph
	// Typically, the current brush transform
	Local,
	// World space transform in unreal units
	World,
	// Transform space of the actor making the query
	// Typically, the voxel actor which is rendering the mesh
	Query
};

UCLASS()
class VOXELGRAPHCORE_API UVoxelSurfaceFunctionLibrary : public UVoxelFunctionLibrary
{
	GENERATED_BODY()

public:
	// Get the bounds of a surface
	// @param	Smoothness	Bounds will be increased by Smoothness. Should be the same as the SmoothUnion smoothness.
	UFUNCTION(Category = "Surface", meta = (AdvancedDisplay = "TransformSpace"))
	FVoxelBox GetSurfaceBounds(
		const FVoxelSurface& Surface,
		float Smoothness = 0.f,
		EVoxelTransformSpace TransformSpace = EVoxelTransformSpace::Local) const;

	// Transform Value from local space into surface space
	// This is automatically applied to Smoothness in SmoothUnion/Intersection
	UFUNCTION(Category = "Surface")
	float TransformToSurfaceSpace(
		const FVoxelSurface& Surface,
		float Value) const;

	UFUNCTION(Category = "Surface")
	FVoxelSurfaceMaterial MergeSurfaceMaterials(
		const FVoxelSurfaceMaterial& A,
		const FVoxelSurfaceMaterial& B,
		const FVoxelFloatBuffer& Alpha) const;

public:
	// Apply the transform of the current graph/actor to the surface
	UFUNCTION(Category = "Surface", meta = (Internal))
	FVoxelSurface ApplyTransform(const FVoxelSurface& Surface) const;

	UFUNCTION(Category = "Surface", meta = (ShowInShortList))
	FVoxelSurface Invert(const FVoxelSurface& Surface) const;

	// Grow the surface by some amount
	UFUNCTION(Category = "Surface", meta = (ShowInShortList))
	FVoxelSurface Grow(
		const FVoxelSurface& Surface,
		float Amount = 100.f) const;

	UFUNCTION(Category = "Surface", meta = (ShowInShortList))
	FVoxelSurface SmoothUnion(
		const FVoxelSurface& A,
		const FVoxelSurface& B,
		float Smoothness = 100.f) const;

	UFUNCTION(Category = "Surface", meta = (ShowInShortList))
	FVoxelSurface SmoothIntersection(
		const FVoxelSurface& A,
		const FVoxelSurface& B,
		float Smoothness = 100.f) const;

	UFUNCTION(Category = "Surface", meta = (ShowInShortList))
	FVoxelSurface SmoothSubtraction(
		const FVoxelSurface& Surface,
		const FVoxelSurface& SurfaceToSubtract,
		float Smoothness = 100.f) const;

public:
	UFUNCTION(Category = "Surface")
	FVoxelSurface MakeSphereSurface(
		const FVector& Center,
		float Radius = 500.f) const;

	UFUNCTION(Category = "Surface")
	FVoxelSurface MakeBoxSurface(
		const FVector& Center,
		const FVector& Extent = FVector(500.f),
		float Smoothness = 100.f) const;

public:
	FVoxelFloatBuffer MakeSphereSurface_Distance(
		const FVector& Center,
		float Radius) const;

	FVoxelFloatBuffer MakeBoxSurface_Distance(
		const FVector& Center,
		const FVector& Extent,
		float Smoothness) const;
};