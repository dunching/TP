// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelPositionNodes.h"
#include "VoxelBufferUtilities.h"
#include "VoxelPositionQueryParameter.h"

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_QueryWithGradientStep, OutData)
{
	const TValue<float> Step = Get(StepPin, Query);

	return VOXEL_ON_COMPLETE(Step)
	{
		const TSharedRef<FVoxelQueryParameters> Parameters = Query.CloneParameters();
		Parameters->Add<FVoxelGradientStepQueryParameter>().Step = Step;
		return Get(DataPin, Query.MakeNewQuery(Parameters));
	};
}

#if WITH_EDITOR
FVoxelPinTypeSet FVoxelNode_QueryWithGradientStep::GetPromotionTypes(const FVoxelPin& Pin) const
{
	return FVoxelPinTypeSet::All();
}

void FVoxelNode_QueryWithGradientStep::PromotePin(FVoxelPin& Pin, const FVoxelPinType& NewType)
{
	GetPin(DataPin).SetType(NewType);
	GetPin(OutDataPin).SetType(NewType);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_QueryWithPosition, OutData)
{
	const TValue<FVoxelVectorBuffer> Positions = Get(PositionPin, Query);

	return VOXEL_ON_COMPLETE(Positions)
	{
		const TSharedRef<FVoxelQueryParameters> Parameters = Query.CloneParameters();
		Parameters->Add<FVoxelPositionQueryParameter>().InitializeSparse(Positions);
		return Get(DataPin, Query.MakeNewQuery(Parameters));
	};
}

#if WITH_EDITOR
FVoxelPinTypeSet FVoxelNode_QueryWithPosition::GetPromotionTypes(const FVoxelPin& Pin) const
{
	return FVoxelPinTypeSet::All();
}

void FVoxelNode_QueryWithPosition::PromotePin(FVoxelPin& Pin, const FVoxelPinType& NewType)
{
	GetPin(DataPin).SetType(NewType);
	GetPin(OutDataPin).SetType(NewType);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_Query2D, OutData)
{
	return FVoxelBufferUtilities::Query2D(*GetCompute(DataPin), Query, ReturnPinType);
}

#if WITH_EDITOR
FVoxelPinTypeSet FVoxelNode_Query2D::GetPromotionTypes(const FVoxelPin& Pin) const
{
	return FVoxelPinTypeSet::AllBuffers();
}

void FVoxelNode_Query2D::PromotePin(FVoxelPin& Pin, const FVoxelPinType& NewType)
{
	GetPin(DataPin).SetType(NewType);
	GetPin(OutDataPin).SetType(NewType);
}
#endif