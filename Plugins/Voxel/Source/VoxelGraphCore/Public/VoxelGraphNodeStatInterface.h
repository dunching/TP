// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelGraphNodeStatInterface.generated.h"

struct IVoxelNodeInterface;

UINTERFACE()
class UVoxelGraphNodeStatInterface : public UInterface
{
	GENERATED_BODY()
};

class VOXELGRAPHCORE_API IVoxelGraphNodeStatInterface
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	struct FStats
	{
		double Time = 0.;
		int64 NumElements = 0;
	};
	FStats Stats;

	static bool bEnableStats;
#endif
};

extern VOXELGRAPHCORE_API FSimpleMulticastDelegate GVoxelOnClearNodeStats;

class VOXELGRAPHCORE_API FVoxelNodeStatScope
{
public:
	FORCEINLINE FVoxelNodeStatScope(const IVoxelNodeInterface& InNode, const int64 InCount)
	{
#if WITH_EDITOR
		if (!IVoxelGraphNodeStatInterface::bEnableStats)
		{
			return;
		}

		Node = &InNode;
		Count = InCount;
		StartTime = FPlatformTime::Seconds();
#endif
	}
	FORCEINLINE ~FVoxelNodeStatScope()
	{
#if WITH_EDITOR
		if (IsEnabled())
		{
			RecordStats(FPlatformTime::Seconds() - StartTime);
		}
#endif
	}

	FORCEINLINE bool IsEnabled() const
	{
#if WITH_EDITOR
		return Node != nullptr;
#else
		return false;
#endif
	}
	FORCEINLINE void SetCount(const int32 NewCount)
	{
#if WITH_EDITOR
		Count = NewCount;
#endif
	}

private:
#if WITH_EDITOR
	const IVoxelNodeInterface* Node = nullptr;
	int64 Count = 0;
	double StartTime = 0;

	void RecordStats(const double Duration) const;
#endif
};