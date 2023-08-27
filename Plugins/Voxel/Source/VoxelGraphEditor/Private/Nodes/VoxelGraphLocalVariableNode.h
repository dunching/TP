// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelGraphParameterNodeBase.h"
#include "VoxelGraphLocalVariableNode.generated.h"

class UVoxelGraphLocalVariableUsageNode;

UCLASS(Abstract)
class UVoxelGraphLocalVariableNode : public UVoxelGraphParameterNodeBase
{
	GENERATED_BODY()

public:
	//~ Begin UVoxelGraphNode Interface
	virtual void PostPasteNode() override;
	//~ End UVoxelGraphNode Interface
};

UCLASS()
class UVoxelGraphLocalVariableDeclarationNode : public UVoxelGraphLocalVariableNode
{
	GENERATED_BODY()

public:
	//~ Begin UVoxelGraphLocalVariableNode Interface
	virtual void AllocateParameterPins(const FVoxelGraphParameter& Parameter) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanJumpToDefinition() const override { return true; }
	virtual void PostReconstructNode() override;
	//~ End UVoxelGraphLocalVariableNode Interface

	UVoxelGraphNode* IsInLoop();
};

UCLASS()
class UVoxelGraphLocalVariableUsageNode : public UVoxelGraphLocalVariableNode
{
	GENERATED_BODY()

public:
	//~ Begin UVoxelGraphLocalVariableNode Interface
	virtual void AllocateParameterPins(const FVoxelGraphParameter& Parameter) override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual bool CanJumpToDefinition() const override { return true; }
	virtual void JumpToDefinition() const override;
	//~ End UVoxelGraphLocalVariableNode Interface

	UVoxelGraphLocalVariableDeclarationNode* FindDeclaration() const;
};