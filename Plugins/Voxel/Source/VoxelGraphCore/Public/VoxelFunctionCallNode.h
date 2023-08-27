// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelParameter.h"
#include "VoxelFunctionCallNode.generated.h"

class FVoxelInlineGraphData;

USTRUCT(meta = (Internal))
struct FVoxelNode_FunctionCall : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

public:
	enum class EMode : uint8
	{
		Macro,
		Template,
		RecursiveTemplate
	};

	void Initialize(
		EMode NewMode,
		const UVoxelGraphInterface& NewDefaultGraphInterface);

	//~ Begin FVoxelNode Interface
	virtual void PreCompile() override;
	virtual FVoxelComputeValue CompileCompute(FName PinName) const override;
	//~ End FVoxelNode Interface

private:
	EMode Mode = {};

	// UPROPERTY for diffing
	UPROPERTY()
	TWeakObjectPtr<const UVoxelGraphInterface> DefaultGraphInterface;

private:
	TSharedPtr<FVoxelComputeInputMap> ComputeInputMap;
	TVoxelMap<FName, FVoxelParameter> PinNameToInputParameter;
	TVoxelMap<FName, FVoxelParameter> PinNameToOutputParameter;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

USTRUCT(meta = (Internal))
struct FVoxelNode_FunctionCallInput_WithoutDefaultPin : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	FName Name;
	FVoxelRuntimePinValue DefaultValue;

	VOXEL_TEMPLATE_OUTPUT_PIN(FVoxelWildcard, Value);

#if WITH_EDITOR
	virtual FVoxelPinTypeSet GetPromotionTypes(const FVoxelPin& Pin) const override
	{
		return FVoxelPinTypeSet::All();
	}
#endif
};

USTRUCT(meta = (Internal))
struct FVoxelNode_FunctionCallInput_WithDefaultPin : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	FName Name;

	VOXEL_TEMPLATE_INPUT_PIN(FVoxelWildcard, Default, nullptr);
	VOXEL_TEMPLATE_OUTPUT_PIN(FVoxelWildcard, Value);

#if WITH_EDITOR
	virtual FVoxelPinTypeSet GetPromotionTypes(const FVoxelPin& Pin) const override
	{
		return FVoxelPinTypeSet::All();
	}
#endif
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

USTRUCT(meta = (Internal))
struct FVoxelNode_FunctionCallOutput : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_TEMPLATE_INPUT_PIN(FVoxelWildcard, Value, nullptr);

#if WITH_EDITOR
	virtual FVoxelPinTypeSet GetPromotionTypes(const FVoxelPin& Pin) const override
	{
		return FVoxelPinTypeSet::All();
	}
#endif
};