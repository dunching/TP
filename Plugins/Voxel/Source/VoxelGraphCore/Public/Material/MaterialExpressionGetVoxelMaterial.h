// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelPinType.h"
#include "VoxelPinValue.h"
#include "VoxelMaterialDefinitionInterface.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialExpressionGetVoxelMaterial.generated.h"

class UVoxelMaterialDefinition;

DECLARE_VOXEL_MEMORY_STAT(VOXELGRAPHCORE_API, STAT_VoxelMaterialTextureMemory, "Voxel Material Texture Memory (GPU)");

UCLASS(Abstract)
class VOXELGRAPHCORE_API UMaterialExpressionGetVoxelMaterial_Base : public UMaterialExpression
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "If not set will query the Voxel Material Ids and linearly blend the output"))
	FExpressionInput MaterialId;

	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<UVoxelMaterialDefinition> MaterialDefinition;

	UPROPERTY(EditAnywhere, Category = "Config")
	FName ParameterName;

public:
	UMaterialExpressionGetVoxelMaterial_Base();

	static const UMaterialExpressionGetVoxelMaterial_Base* GetTemplate(const FVoxelPinType& Type);

public:
	virtual FVoxelPinType GetVoxelParameterType() const VOXEL_PURE_VIRTUAL({});
	virtual UScriptStruct* GetVoxelParameterDataType() const VOXEL_PURE_VIRTUAL({});

	struct FInstance
	{
		UObject* DebugObject = nullptr;
		int32 Index = 0;
		FVoxelTerminalPinValue Value;
	};
	virtual void UpdateVoxelParameterData(
		FName DebugName,
		const TVoxelArray<FInstance>& Instances,
		TVoxelInstancedStruct<FVoxelMaterialParameterData>& InOutParameterData) const VOXEL_PURE_VIRTUAL();

#if WITH_EDITOR
	virtual void CompileVoxel(
		FMaterialCompiler& Compiler,
		UMaterialExpressionCustom& Custom,
		const FVoxelMaterialParameterData* ParameterData,
		FName Name,
		int32 MaterialIdValue,
		int32 PreviewMaterialIdValue,
		TArray<int32>& Inputs) VOXEL_PURE_VIRTUAL();
#endif

public:
	virtual UObject* GetReferencedTexture() const override;
	virtual bool CanReferenceTexture() const override { return true; }

#if WITH_EDITOR
	//~ Begin UMaterialExpression Interface
	virtual int32 Compile(FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	//~ End UMaterialExpression Interface
#endif
};