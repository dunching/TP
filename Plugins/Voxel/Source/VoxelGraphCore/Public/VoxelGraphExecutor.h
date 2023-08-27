// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelGraphExecutor.generated.h"

struct FVoxelRootNode;
class FVoxelGraphExecutorManager;

struct VOXELGRAPHCORE_API FVoxelGraphExecutorInfo
{
#if WITH_EDITOR
	TSharedPtr<const Voxel::Graph::FGraph> Graph_EditorOnly;
#endif
	TSet<FObjectKey> ReferencedObjects;
	TSet<TWeakObjectPtr<const UVoxelGraphInterface>> ReferencedGraphs;

	VOXEL_COUNT_INSTANCES();
};

class VOXELGRAPHCORE_API FVoxelGraphExecutor
{
public:
	const TSharedRef<const FVoxelGraphExecutorInfo> Info;
	const TVoxelAddOnlySet<TSharedPtr<const FVoxelNode>> Nodes;

	VOXEL_COUNT_INSTANCES();

	explicit FVoxelGraphExecutor(
		const TSharedPtr<const FVoxelRootNode>& RootNode,
		const TSharedRef<const FVoxelGraphExecutorInfo>& Info,
		TVoxelAddOnlySet<TSharedPtr<const FVoxelNode>>&& Nodes)
		: Info(Info)
		, Nodes(MoveTemp(Nodes))
		, RootNode(RootNode)
	{
	}

	FVoxelFutureValue Execute(const FVoxelQuery& Query) const;

private:
	const TSharedPtr<const FVoxelRootNode> RootNode;

};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelGraphExecutorRef
{
	GENERATED_BODY()

	TSharedPtr<const FVoxelGraphExecutor> Executor;
};

DECLARE_UNIQUE_VOXEL_ID(FVoxelGraphExecutorGlobalId);

extern VOXELGRAPHCORE_API FVoxelGraphExecutorManager* GVoxelGraphExecutorManager;

// Will keep pin default value objects alive
class VOXELGRAPHCORE_API FVoxelGraphExecutorManager
	: public FVoxelTicker
	, public FGCObject
{
public:
	FVoxelGraphExecutorGlobalId GlobalId;

	FVoxelGraphExecutorManager() = default;

	//~ Begin FVoxelTicker Interface
	virtual void Tick() override;
	//~ End FVoxelTicker Interface

	//~ Begin FGCObject Interface
	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FGCObject Interface

	void RecompileAll();
	void ForceTranslateAll();
	void CleanupOldExecutors(double TimeInSeconds);
	void OnGraphChanged_GameThread(const UVoxelGraphInterface& Graph);

	TSharedRef<const FVoxelComputeValue> MakeCompute_GameThread(
		const FVoxelPinType Type,
		const FVoxelGraphPinRef& Ref,
		bool bReturnExecutor = false);

private:
	class FExecutorRef : public TSharedFromThis<FExecutorRef>
	{
	public:
		const FVoxelGraphPinRef Ref;
		const TSharedRef<FVoxelDependency> Dependency;

		// Used for diffing & referenced objects, always alive
		TSharedPtr<const FVoxelGraphExecutorInfo> ExecutorInfo_GameThread;

		double LastUsedTime = FPlatformTime::Seconds();
		// Might hold references to other ExecutorRefs, will be cleared if unused after a few seconds
		// to avoid circular dependencies
		TOptional<TSharedPtr<const FVoxelGraphExecutor>> Executor_GameThread;

		explicit FExecutorRef(const FVoxelGraphPinRef& Ref);

		TVoxelFutureValue<FVoxelGraphExecutorRef> GetExecutor();
		void SetExecutor(const TSharedPtr<const FVoxelGraphExecutor>& NewExecutor);

	private:
		FVoxelFastCriticalSection CriticalSection;
		TWeakPtr<const FVoxelGraphExecutor> WeakExecutor_RequiresLock;
		TSharedPtr<TVoxelFutureValueState<FVoxelGraphExecutorRef>> FutureValueState_RequiresLock;
	};
	TMap<FVoxelGraphPinRef, TSharedPtr<FExecutorRef>> ExecutorRefs_GameThread;
	TQueue<TSharedPtr<FExecutorRef>, EQueueMode::Mpsc> ExecutorRefsToUpdate;

	TSharedRef<FExecutorRef> MakeExecutorRef_GameThread(const FVoxelGraphPinRef& Ref);

	friend class FExecutorRef;
};