// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphParameterNodeBase.h"
#include "VoxelGraphSchema.h"

FVoxelGraphParameter* UVoxelGraphParameterNodeBase::GetParameter() const
{
	UVoxelGraph* Graph = GetTypedOuter<UVoxelGraph>();
	if (!ensure(Graph))
	{
		return nullptr;
	}

	return Graph->FindParameterByGuid(Guid);
}

void UVoxelGraphParameterNodeBase::AllocateDefaultPins()
{
	if (const FVoxelGraphParameter* Parameter = GetParameter())
	{
		AllocateParameterPins(*Parameter);
		CachedParameter = *Parameter;
	}
	else
	{
		AllocateParameterPins(CachedParameter);
		for (UEdGraphPin* Pin : Pins)
		{
			Pin->bOrphanedPin = true;
		}
	}

	Super::AllocateDefaultPins();
}

bool UVoxelGraphParameterNodeBase::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA<UVoxelGraphSchema>();
}

void UVoxelGraphParameterNodeBase::PrepareForCopying()
{
	Super::PrepareForCopying();

	const FVoxelGraphParameter* Parameter = GetParameter();
	if (!ensure(Parameter))
	{
		return;
	}

	CachedParameter = *Parameter;
}

const FVoxelGraphParameter& UVoxelGraphParameterNodeBase::GetParameterSafe() const
{
	if (FVoxelGraphParameter* Parameter = GetParameter())
	{
		return *Parameter;
	}

	return CachedParameter;
}