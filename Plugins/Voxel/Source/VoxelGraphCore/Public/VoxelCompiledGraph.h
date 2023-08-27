// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelPinType.h"
#include "VoxelPinValue.h"
#include "VoxelGraphNodeRef.h"
#include "VoxelCompiledGraph.generated.h"

struct FVoxelNode;
class UVoxelGraphInterface;

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelCompiledPinRef
{
	GENERATED_BODY()

	UPROPERTY()
	FName NodeId;

	UPROPERTY()
	FName PinName;

	UPROPERTY()
	bool bIsInput = false;
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelCompiledPin
{
	GENERATED_BODY()

	UPROPERTY()
	FVoxelPinType Type;

	UPROPERTY()
	FName PinName;

	UPROPERTY()
	FVoxelPinValue DefaultValue;

	UPROPERTY()
	TArray<FVoxelCompiledPinRef> LinkedTo;
};

UENUM()
enum class EVoxelCompiledNodeType : uint8
{
	Struct,
	Macro,
	Parameter,
	Input,
	Output
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelCompiledNode
{
	GENERATED_BODY()

	UPROPERTY()
	FVoxelGraphNodeRef Ref;

	UPROPERTY()
	EVoxelCompiledNodeType Type = {};

	UPROPERTY()
#if CPP
	TVoxelInstancedStruct<FVoxelNode> Struct;
#else
	FVoxelInstancedStruct Struct;
#endif

	UPROPERTY()
	TObjectPtr<UVoxelGraphInterface> Graph;

	UPROPERTY()
	FGuid Guid;

	UPROPERTY()
	TMap<FName, FVoxelCompiledPin> InputPins;

	UPROPERTY()
	TMap<FName, FVoxelCompiledPin> OutputPins;

	FVoxelCompiledPin* FindPin(const FName PinName, const bool bIsInput)
	{
		TMap<FName, FVoxelCompiledPin>& Pins = bIsInput ? InputPins : OutputPins;
		return Pins.Find(PinName);
	}
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelCompiledGraph
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsValid = false;

	UPROPERTY()
	TMap<FName, FVoxelCompiledNode> Nodes;

	FVoxelCompiledPin* FindPin(const FVoxelCompiledPinRef& Ref)
	{
		FVoxelCompiledNode* Node = Nodes.Find(Ref.NodeId);
		if (!Node)
		{
			return nullptr;
		}

		TMap<FName, FVoxelCompiledPin>& Pins = Ref.bIsInput ? Node->InputPins : Node->OutputPins;
		return Pins.Find(Ref.PinName);
	}
	const FVoxelCompiledPin* FindPin(const FVoxelCompiledPinRef& Ref) const
	{
		return ConstCast(this)->FindPin(Ref);
	}
};