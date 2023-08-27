// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

#if WITH_EDITOR
class VOXELGRAPHCORE_API FVoxelSourceParser
{
public:
	static FVoxelSourceParser& Get();

	FString GetPinTooltip(UScriptStruct* NodeStruct, FName PinName);
	FString GetPropertyDefault(UFunction* Function, FName PropertyName);

private:
	TMap<UScriptStruct*, TMap<FName, FString>> NodeToPinToTooltip;
	TMap<UFunction*, TMap<FName, FString>> FunctionToPropertyToDefault;

	void BuildPinTooltip(UScriptStruct* NodeStruct);
};
#endif