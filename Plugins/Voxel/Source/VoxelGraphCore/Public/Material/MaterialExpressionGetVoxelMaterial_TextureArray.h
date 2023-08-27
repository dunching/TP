// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Engine/Texture2D.h"
#include "Material/MaterialExpressionGetVoxelMaterial.h"
#include "MaterialExpressionGetVoxelMaterial_TextureArray.generated.h"

UENUM()
enum class EVoxelTextureArrayCompression : uint8
{
	DXT1,
	DXT5,
	BC5,
};

USTRUCT()
struct FVoxelMaterialParameterData_TextureArray : public FVoxelMaterialParameterData
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Config")
	int32 TextureSize = 1024;

	UPROPERTY(EditAnywhere, Category = "Config")
	int32 LastMipTextureSize = 16;

	UPROPERTY(EditAnywhere, Category = "Config")
	EVoxelTextureArrayCompression Compression = EVoxelTextureArrayCompression::DXT1;

	int32 GetNumMips() const
	{
		ensureVoxelSlow(TextureSize % LastMipTextureSize == 0);
		return 1 + FVoxelUtilities::ExactLog2(TextureSize / LastMipTextureSize);
	}

	virtual void Fixup() override;

public:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> IndirectionTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2DArray> TextureArray;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> TextureArrayValues;

	virtual void CacheParameters(
		FName Name,
		FCachedParameters& InOutParameters) const override;
};

UENUM()
enum class EVoxelTextureArraySamplerType
{
	Color,
	Normal
};

UCLASS()
class VOXELGRAPHCORE_API UMaterialExpressionGetVoxelMaterial_TextureArray : public UMaterialExpressionGetVoxelMaterial_Base
{
	GENERATED_BODY()

public:
	UMaterialExpressionGetVoxelMaterial_TextureArray();

	UPROPERTY()
	FExpressionInput UVs;

	// If MaterialDefinition is null, this sampler type will be used
	// Otherwise it will be automatically derived from the material definition parameter type
	UPROPERTY(EditAnywhere, Category = "Config")
	EVoxelTextureArraySamplerType DefaultSamplerType = EVoxelTextureArraySamplerType::Color;

	virtual UObject* GetReferencedTexture() const override;
	virtual FVoxelPinType GetVoxelParameterType() const override
	{
		return FVoxelPinType::Make<UTexture2D>();
	}
	virtual UScriptStruct* GetVoxelParameterDataType() const override
	{
		return FVoxelMaterialParameterData_TextureArray::StaticStruct();
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
#endif
};