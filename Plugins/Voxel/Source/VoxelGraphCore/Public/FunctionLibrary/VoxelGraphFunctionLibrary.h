// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Buffer/VoxelBaseBuffers.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelGraphFunctionLibrary.generated.h"

class AVoxelActor;

UCLASS()
class VOXELGRAPHCORE_API UVoxelGraphFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, DisplayName = "Query Voxel Channel", CustomThunk, Category = "Voxel", meta = (WorldContext = "WorldContextObject", CustomStructureParam = "Value", BlueprintInternalUseOnly = "true", AdvancedDisplay = "LOD, GradientStep"))
	static void K2_QueryVoxelChannel(
		int32& Value,
		UObject* WorldContextObject,
		FName Channel,
		FVector Position,
		int32 LOD = 0,
		float GradientStep = 100.f)
	{
		checkf(false, TEXT("Use QueryVoxelChannel instead"));
	}
	DECLARE_FUNCTION(execK2_QueryVoxelChannel);

	UFUNCTION(BlueprintCallable, DisplayName = "Multi Query Voxel Channel", CustomThunk, Category = "Voxel", meta = (WorldContext = "WorldContextObject", CustomStructureParam = "Value", BlueprintInternalUseOnly = "true", AdvancedDisplay = "LOD, GradientStep"))
	static void K2_MultiQueryVoxelChannel(
		int32& Value,
		UObject* WorldContextObject,
		FName Channel,
		TArray<FVector> Positions,
		int32 LOD = 0,
		float GradientStep = 100.f)
	{
		checkf(false, TEXT("Use QueryVoxelChannel instead"));
	}
	DECLARE_FUNCTION(execK2_MultiQueryVoxelChannel);

	// Will return a buffer of the same size as Positions
	// If Positions.Num() == 1, you will want to do
	// QueryVoxelChannel(...).Get<FVoxelFloatBuffer>().GetConstant()
	static FVoxelRuntimePinValue QueryVoxelChannel(
		const UObject* WorldContextObject,
		FName ChannelName,
		TConstVoxelArrayView<FVector3f> Positions,
		int32 LOD = 0,
		float GradientStep = 100.f);

public:
	UFUNCTION(BlueprintCallable, Category = "Voxel", meta = (WorldContext = "WorldContextObject"))
	static void RegisterVoxelChannel(
		const UObject* WorldContextObject,
		FName ChannelName,
		FVoxelPinType Type,
		FVoxelPinValue DefaultValue);
};