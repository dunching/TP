// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelParameterFunctionLibrary.generated.h"

class UVoxelParameterContainer;

UCLASS()
class VOXELGRAPHCORE_API UVoxelParameterFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, DisplayName = "Get Voxel Parameter", CustomThunk, Category = "Voxel|Parameters", meta = (CustomStructureParam = "Value", BlueprintInternalUseOnly = "true"))
	static void K2_GetVoxelParameter(
		UVoxelParameterContainer* ParameterContainer,
		FName Name,
		int32& Value);

	static void GetVoxelParameterImpl(
		UVoxelParameterContainer* ParameterContainer,
		FName Name,
		void* OutData,
		const FProperty* PropertyForTypeCheck);

	DECLARE_FUNCTION(execK2_GetVoxelParameter);

public:
	UFUNCTION(BlueprintCallable, DisplayName = "Set Voxel Parameter", CustomThunk, Category = "Voxel|Parameters", meta = (AutoCreateRefTerm = "Value", CustomStructureParam = "Value,OutValue", BlueprintInternalUseOnly = "true"))
	static void K2_SetVoxelParameter(
		UVoxelParameterContainer* ParameterContainer,
		FName Name,
		const int32& Value,
		int32& OutValue);

	static void SetVoxelParameterImpl(
		UVoxelParameterContainer* ParameterContainer,
		FName Name,
		const void* ValuePtr,
		const FProperty* ValueProperty);

	DECLARE_FUNCTION(execK2_SetVoxelParameter);
};