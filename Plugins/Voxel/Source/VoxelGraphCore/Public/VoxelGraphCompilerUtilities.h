// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelGraphExecutor.h"
#include "VoxelCompilationGraph.h"

BEGIN_VOXEL_NAMESPACE(Graph)

class VOXELGRAPHCORE_API FCompilationScope
{
public:
	FCompilationScope();
	~FCompilationScope();

private:
	struct FCache
	{
		TSharedPtr<const FGraph> Graph;
		TSet<FObjectKey> ReferencedObjects;
		TSet<TWeakObjectPtr<const UVoxelGraphInterface>> ReferencedGraphs;
	};
	TVoxelMap<const UVoxelGraph*, TSharedPtr<FCache>> CacheMap;

	friend struct FCompilerUtilities;
};

struct VOXELGRAPHCORE_API FCompilerUtilities
{
public:
#if WITH_EDITOR
	static TFunction<void(UVoxelGraph&)> ForceTranslateGraph_EditorOnly;
	static TFunction<void(UVoxelGraph&)> RaiseOrphanWarnings_EditorOnly;
#endif

	static bool IsNodeDeleted(const FVoxelGraphNodeRef& NodeRef);
	static void TranslateIfNeeded(const UVoxelGraph& Graph);
	static TSharedPtr<FVoxelGraphExecutor> CreateExecutor(const FVoxelGraphPinRef& GraphPinRef);

public:
	static TSharedPtr<const FGraph> TranslateCompiledGraph(
		const UVoxelGraph& VoxelGraph,
		TSet<FObjectKey>* OutReferencedObjects = nullptr,
		TSet<TWeakObjectPtr<const UVoxelGraphInterface>>* OutReferencedGraphs = nullptr);

	static TSharedPtr<FGraph> TranslateCompiledGraphImpl(
		const UVoxelGraph& VoxelGraph,
		TSet<FObjectKey>& OutReferencedObjects,
		TSet<TWeakObjectPtr<const UVoxelGraphInterface>>& OutReferencedGraphs);

	static void AddExecOutput(FGraph& Graph, const UVoxelGraph& VoxelGraph);
	static void CheckWildcards(const FGraph& Graph);
	static void CheckNoDefault(const FGraph& Graph);
	static void ReplaceTemplates(FGraph& Graph);
	static bool ReplaceTemplatesImpl(FGraph& Graph);
	static void InitializeTemplatesPassthroughNodes(FGraph& Graph, FNode& Node);

	static void RemovePassthroughs(FGraph& Graph);
	static void DisconnectVirtualPins(FGraph& Graph);

public:
	static void RemoveUnusedNodes(FGraph& Graph);

	static TSet<const FNode*> FindNodesPredecessors(
		const TArray<const FNode*>& Nodes,
		const TFunction<bool(const FNode&)>& ShouldFindPredecessors = nullptr);

	static TArray<const FNode*> SortNodes(const TArray<const FNode*>& Nodes);

	static TArray<FNode*> SortNodes(const TArray<FNode*>& Nodes)
	{
		return ReinterpretCastArray<FNode*>(SortNodes(ReinterpretCastArray<const FNode*>(Nodes)));
	}
};

END_VOXEL_NAMESPACE(Graph)