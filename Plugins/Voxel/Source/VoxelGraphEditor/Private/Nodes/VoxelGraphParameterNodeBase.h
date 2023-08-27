// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelGraph.h"
#include "VoxelGraphNode.h"
#include "VoxelGraphParameterNodeBase.generated.h"

UCLASS()
class UVoxelGraphParameterNodeBase : public UVoxelGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGuid Guid;

	UPROPERTY()
	FVoxelGraphParameter CachedParameter;

	FVoxelGraphParameter* GetParameter() const;
	const FVoxelGraphParameter& GetParameterSafe() const;

	//~ Begin UVoxelGraphNode Interface
	virtual void AllocateDefaultPins() final override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;
	virtual void PrepareForCopying() override;
	//~ End UVoxelGraphNode Interface

protected:
	virtual void AllocateParameterPins(const FVoxelGraphParameter& Parameter) VOXEL_PURE_VIRTUAL();
};