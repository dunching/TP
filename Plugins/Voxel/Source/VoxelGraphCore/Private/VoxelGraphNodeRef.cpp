// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphNodeRef.h"
#include "VoxelGraph.h"
#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#endif

const FName FVoxelNodeNames::Builtin = "Builtin";
const FName FVoxelNodeNames::ExecuteNodeId = "ExecuteNode";
const FName FVoxelNodeNames::MergeNodeId = "MergeNode";
const FName FVoxelNodeNames::GetPreviousOutputNodeId = "GetPreviousOutputNode";
const FName FVoxelNodeNames::MacroTemplateInput = "Template_Graph";
const FName FVoxelNodeNames::MacroRecursiveTemplateInput = "RecursiveTemplate_Graphs";

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelGraph* FVoxelGraphNodeRef::GetGraph() const
{
	const UVoxelGraphInterface* GraphInterface = Graph.Get();
	if (!GraphInterface)
	{
		return nullptr;
	}
	return GraphInterface->GetGraph();
}

#if WITH_EDITOR
UEdGraphNode* FVoxelGraphNodeRef::GetGraphNode_EditorOnly() const
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	if (EdGraphNodeName_EditorOnly == FVoxelNodeNames::Builtin)
	{
		return nullptr;
	}

	const UVoxelGraph* GraphObject = GetGraph();
	if (!GraphObject)
	{
		return nullptr;
	}

	const UEdGraph* EdGraph = GraphObject->MainEdGraph;
	if (!EdGraph)
	{
		// New graph
		return nullptr;
	}

	for (UEdGraphNode* Node : EdGraph->Nodes)
	{
		if (Node->GetFName() == EdGraphNodeName_EditorOnly)
		{
			return Node;
		}
	}

	ensureVoxelSlow(false);
	return nullptr;
}
#endif

FVoxelGraphNodeRef FVoxelGraphNodeRef::WithSuffix(const FString& Suffix) const
{
	FVoxelGraphNodeRef Result;
	Result.Graph = Graph;
	Result.NodeId = NodeId + "_" + Suffix;
	Result.TemplateInstance = TemplateInstance;
	Result.DebugName = DebugName + " (" + Suffix + ")";
#if WITH_EDITOR
	Result.EdGraphNodeName_EditorOnly = EdGraphNodeName_EditorOnly;
#endif
	return Result;
}

FString FVoxelGraphPinRef::ToString() const
{
	const UVoxelGraphInterface* Graph = Node.Graph.Get();
	return FString::Printf(TEXT("%s.%s.%s"),
		Graph ? *Graph->GetPathName() : TEXT("<null>"),
		*Node.DebugName.ToString(),
		*PinName.ToString());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelNodePath::NetSerialize(FArchive& Ar, UPackageMap& Map)
{
	if (Ar.IsSaving())
	{
		int32 Num = NodeRefs.Num();
		Ar << Num;

		for (FVoxelGraphNodeRef& NodeRef : NodeRefs)
		{
			UObject* Object = ConstCast(NodeRef.Graph.Get());
			ensure(Object);

			if (!ensure(Map.SerializeObject(Ar, UVoxelGraphInterface::StaticClass(), Object)))
			{
				return false;
			}

			Ar << NodeRef.NodeId;
			ensure(NodeRef.TemplateInstance == 0);
		}

		return true;
	}
	else if (Ar.IsLoading())
	{
		int32 Num = NodeRefs.Num();
		Ar << Num;

		NodeRefs.SetNum(Num);

		for (FVoxelGraphNodeRef& NodeRef : NodeRefs)
		{
			UObject* Object = nullptr;
			if (!ensure(Map.SerializeObject(Ar, UVoxelGraphInterface::StaticClass(), Object)) ||
				!ensure(Object))
			{
				return false;
			}

			NodeRef.Graph = CastChecked<UVoxelGraphInterface>(Object);
			Ar << NodeRef.NodeId;
		}

		return true;
	}
	else
	{
		ensure(false);
		return false;
	}
}

uint32 GetTypeHash(const FVoxelNodePath& Path)
{
	checkStatic(sizeof(FMinimalName) == sizeof(uint64));

	TVoxelArray<FMinimalName, TVoxelInlineAllocator<16>> Names;
	for (const FVoxelGraphNodeRef& NodeRef : Path.NodeRefs)
	{
		Names.Add(FMinimalName(NodeRef.NodeId));
	}

	return FVoxelUtilities::MurmurHashBytes(
		MakeByteVoxelArrayView(Names),
		Names.Num());
}