// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMergeSpawnableNode.h"

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_MergeSpawnable, Out)
{
	const TVoxelArray<TValue<FVoxelSpawnable>> Inputs = Get(InputPins, Query);

	return VOXEL_ON_COMPLETE(Inputs)
	{
		const TSharedRef<FVoxelMergedSpawnable> MergedSpawnable = MakeVoxelSpawnable<FVoxelMergedSpawnable>(Query, GetNodeRef());
		for (const TSharedRef<const FVoxelSpawnable>& Input : Inputs)
		{
			if (!Input->IsValid())
			{
				continue;
			}
			MergedSpawnable->Spawnables.Add(ConstCast(Input));
		}

		if (MergedSpawnable->Spawnables.Num() == 0)
		{
			return {};
		}
		if (MergedSpawnable->Spawnables.Num() == 1)
		{
			for (const TSharedPtr<FVoxelSpawnable>& Spawnable : MergedSpawnable->Spawnables)
			{
				return Spawnable;
			}
		}

		return MergedSpawnable;
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMergedSpawnable::Create_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();

	for (const TSharedPtr<FVoxelSpawnable>& Spawnable : Spawnables)
	{
		Spawnable->CallCreate_AnyThread();
	}
}

void FVoxelMergedSpawnable::Destroy_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();

	for (const TSharedPtr<FVoxelSpawnable>& Spawnable : Spawnables)
	{
		Spawnable->CallDestroy_AnyThread();
	}
}

FVoxelOptionalBox FVoxelMergedSpawnable::GetBounds() const
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelOptionalBox Result;
	for (const TSharedPtr<FVoxelSpawnable>& Spawnable : Spawnables)
	{
		Result += Spawnable->GetBounds();
	}
	return Result;
}