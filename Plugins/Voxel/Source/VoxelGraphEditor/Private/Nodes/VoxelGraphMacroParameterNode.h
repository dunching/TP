// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelGraphParameterNodeBase.h"
#include "VoxelGraphMacroParameterNode.generated.h"

UCLASS()
class UVoxelGraphMacroParameterNode : public UVoxelGraphParameterNodeBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	EVoxelGraphParameterType Type;

	//~ Begin UVoxelGraphNode Interface
	virtual void PostPasteNode() override;

	virtual void AllocateParameterPins(const FVoxelGraphParameter& Parameter) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	//~ End UVoxelGraphNode Interface
};