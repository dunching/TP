// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphNodeStatInterface.h"
#include "VoxelCompilationGraph.h"
#include "VoxelNode.h"
#include "EdGraph/EdGraphNode.h"

FSimpleMulticastDelegate GVoxelOnClearNodeStats;

#if WITH_EDITOR
class FVoxelNodeStatManager : public FVoxelTicker
{
public:
	struct FQueuedStat
	{
		FVoxelGraphNodeRef NodeRef;
		double Duration = 0.;
		int64 Count = 0;
	};

	TQueue<FQueuedStat, EQueueMode::Mpsc> Queue;

	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		FQueuedStat Stat;
		while (Queue.Dequeue(Stat))
		{
			UEdGraphNode* GraphNode = Stat.NodeRef.GetGraphNode_EditorOnly();
			if (!ensure(GraphNode))
			{
				continue;
			}

			IVoxelGraphNodeStatInterface* StatInterface = Cast<IVoxelGraphNodeStatInterface>(GraphNode);
			if (!ensure(StatInterface))
			{
				continue;
			}

			StatInterface->Stats.Time += Stat.Duration;
			StatInterface->Stats.NumElements += Stat.Count;
		}
	}
	//~ End FVoxelTicker Interface
};

FVoxelNodeStatManager* GVoxelNodeStatManager = nullptr;

VOXEL_RUN_ON_STARTUP_GAME(RegisterVoxelNodeStatManager)
{
	GVoxelNodeStatManager = new FVoxelNodeStatManager();
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITORONLY_DATA
bool IVoxelGraphNodeStatInterface::bEnableStats = false;
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void FVoxelNodeStatScope::RecordStats(const double Duration) const
{
	GVoxelNodeStatManager->Queue.Enqueue(
	{
		Node->GetNodeRef(),
		Duration,
		Count
	});
}
#endif