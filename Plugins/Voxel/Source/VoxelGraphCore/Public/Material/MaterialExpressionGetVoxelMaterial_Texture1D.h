// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Material/MaterialExpressionGetVoxelMaterial.h"
#include "MaterialExpressionGetVoxelMaterial_Texture1D.generated.h"

USTRUCT()
struct FVoxelMaterialParameterData_Texture1D : public FVoxelMaterialParameterData
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	VOXEL_ALLOCATED_SIZE_TRACKER_CUSTOM(STAT_VoxelMaterialTextureMemory, TextureMemory);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Texture;

	UPROPERTY(Transient)
	TArray<uint8> Data;

	virtual void CacheParameters(
		FName Name,
		FCachedParameters& InOutParameters) const override;
};

UCLASS(Abstract)
class VOXELGRAPHCORE_API UMaterialExpressionGetVoxelMaterial_Texture1D : public UMaterialExpressionGetVoxelMaterial_Base
{
	GENERATED_BODY()

public:
	virtual EPixelFormat GetVoxelTexturePixelFormat() const VOXEL_PURE_VIRTUAL({});
	virtual UScriptStruct* GetVoxelParameterDataType() const override
	{
		return FVoxelMaterialParameterData_Texture1D::StaticStruct();
	}
	virtual void UpdateVoxelParameterData(
		FName DebugName,
		const TVoxelArray<FInstance>& Instances,
		TVoxelInstancedStruct<FVoxelMaterialParameterData>& InOutParameterData) const override;

#if WITH_EDITOR
	virtual void CompileVoxel(
		FMaterialCompiler& Compiler,
		UMaterialExpressionCustom& Custom,
		const FVoxelMaterialParameterData* ParameterData,
		FName Name,
		int32 MaterialIdValue,
		int32 PreviewMaterialIdValue,
		TArray<int32>& Inputs) override;

	virtual ECustomMaterialOutputType GetCustomOutputType() const VOXEL_PURE_VIRTUAL({});
	virtual FString GenerateHLSL(const FString& Value)const VOXEL_PURE_VIRTUAL({});
#endif
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS()
class VOXELGRAPHCORE_API UMaterialExpressionGetVoxelMaterial_Scalar : public UMaterialExpressionGetVoxelMaterial_Texture1D
{
	GENERATED_BODY()

public:
	virtual EPixelFormat GetVoxelTexturePixelFormat() const override
	{
		return PF_R32_FLOAT;
	}
	virtual FVoxelPinType GetVoxelParameterType() const override
	{
		return FVoxelPinType::Make<float>();
	}

#if WITH_EDITOR
	virtual ECustomMaterialOutputType GetCustomOutputType() const override
	{
		return CMOT_Float1;
	}
	virtual FString GenerateHLSL(const FString& Value) const override
	{
		return Value + ".r";
	}
#endif
};

UCLASS()
class VOXELGRAPHCORE_API UMaterialExpressionGetVoxelMaterial_Color : public UMaterialExpressionGetVoxelMaterial_Texture1D
{
	GENERATED_BODY()

public:
	UMaterialExpressionGetVoxelMaterial_Color();

	virtual EPixelFormat GetVoxelTexturePixelFormat() const override
	{
		return PF_A32B32G32R32F;
	}
	virtual FVoxelPinType GetVoxelParameterType() const override
	{
		return FVoxelPinType::Make<FLinearColor>();
	}

#if WITH_EDITOR
	virtual ECustomMaterialOutputType GetCustomOutputType() const override
	{
		return CMOT_Float4;
	}
	virtual FString GenerateHLSL(const FString& Value) const override
	{
		return Value + ".rgba";
	}
#endif
};