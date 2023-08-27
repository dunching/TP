// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelNodeMessages.h"
#include "VoxelNode.h"
#include "VoxelGraph.h"
#include "VoxelGraphExecutor.h"
#include "VoxelCompilationGraph.h"
#include "EdGraph/EdGraphNode.h"

VOXEL_RUN_ON_STARTUP_GAME(RegisterGatherCallstack)
{
	FVoxelMessages::OnGatherCallstacks.AddLambda([](TVoxelArray<TSharedPtr<FVoxelMessageBuilder>>& OutCallstacks)
	{
		const FVoxelQueryScope* QueryScope = FVoxelQueryScope::TryGet();
		if (!QueryScope)
		{
			return;
		}

		if (!QueryScope->Query)
		{
			if (!QueryScope->QueryContext)
			{
				return;
			}

			const TSharedRef<FVoxelMessageBuilder> InstanceBuilder = MakeVoxelShared<FVoxelMessageBuilder>(EVoxelMessageSeverity::Info, "Instance: {0}");
			FVoxelMessageArgProcessor::ProcessArg(*InstanceBuilder, QueryScope->QueryContext->GetRuntimeInfoRef().GetInstance());
			OutCallstacks.Add(InstanceBuilder);

			const TSharedRef<FVoxelMessageBuilder> CallstackBuilder = MakeVoxelShared<FVoxelMessageBuilder>(EVoxelMessageSeverity::Info, "Callstack: {0}");
			FVoxelMessageArgProcessor::ProcessArg(*CallstackBuilder, *QueryScope->QueryContext->Callstack);
			OutCallstacks.Add(CallstackBuilder);

			return;
		}

		const TWeakObjectPtr<UObject> LocalInstance = QueryScope->Query->GetInfo(EVoxelQueryInfo::Local).GetInstance();
		const TWeakObjectPtr<UObject> QueryInstance = QueryScope->Query->GetInfo(EVoxelQueryInfo::Query).GetInstance();

		if (LocalInstance != QueryInstance)
		{
			const TSharedRef<FVoxelMessageBuilder> LocalBuilder = MakeVoxelShared<FVoxelMessageBuilder>(EVoxelMessageSeverity::Info, "Instance (local): {0}");
			FVoxelMessageArgProcessor::ProcessArg(*LocalBuilder, LocalInstance);

			const TSharedRef<FVoxelMessageBuilder> QueryBuilder = MakeVoxelShared<FVoxelMessageBuilder>(EVoxelMessageSeverity::Info, "Instance (query): {0}");
			FVoxelMessageArgProcessor::ProcessArg(*QueryBuilder, QueryInstance);

			OutCallstacks.Add(LocalBuilder);
			OutCallstacks.Add(QueryBuilder);
		}
		else
		{
			const TSharedRef<FVoxelMessageBuilder> InstanceBuilder = MakeVoxelShared<FVoxelMessageBuilder>(EVoxelMessageSeverity::Info, "Instance: {0}");
			FVoxelMessageArgProcessor::ProcessArg(*InstanceBuilder, LocalInstance);
			OutCallstacks.Add(InstanceBuilder);
		}

		const TSharedRef<FVoxelMessageBuilder> CallstackBuilder = MakeVoxelShared<FVoxelMessageBuilder>(EVoxelMessageSeverity::Info, "Callstack: {0}");
		FVoxelMessageArgProcessor::ProcessArg(*CallstackBuilder, QueryScope->Query->GetCallstack());
		OutCallstacks.Add(CallstackBuilder);
	});
}

void TVoxelMessageArgProcessor<FVoxelNodeRuntime>::ProcessArg(FVoxelMessageBuilder& Builder, const FVoxelNodeRuntime* NodeRuntime)
{
	if (!ensure(NodeRuntime))
	{
		FVoxelMessageArgProcessor::ProcessArg(Builder, "Null");
		return;
	}

	{
		static FVoxelCriticalSection CriticalSection;
		VOXEL_SCOPE_LOCK(CriticalSection);

		FVoxelNodeRuntime::FErrorMessage& ErrorMessage = NodeRuntime->ErrorMessages.FindOrAdd(Builder.Format);

		// Only silence if we didn't recompile anything since the error was raised
		if (ErrorMessage.ExecutorGlobalId == GVoxelGraphExecutorManager->GlobalId)
		{
			// Don't silence if we're building the same error message
			// (ie, if a node is printed twice in a message)
			if (ErrorMessage.FirstBuilderId != Builder.BuilderId)
			{
				Builder.Silence();
			}
		}
		else
		{
			ErrorMessage.ExecutorGlobalId = GVoxelGraphExecutorManager->GlobalId;
			ErrorMessage.FirstBuilderId = Builder.BuilderId;
		}
	}

	FVoxelMessageArgProcessor::ProcessArg(Builder, NodeRuntime->GetNodeRef());
}

void TVoxelMessageArgProcessor<FVoxelNode>::ProcessArg(FVoxelMessageBuilder& Builder, const FVoxelNode* Node)
{
	if (!ensure(Node))
	{
		FVoxelMessageArgProcessor::ProcessArg(Builder, "Null");
		return;
	}

	FVoxelMessageArgProcessor::ProcessArg(Builder, Node->GetNodeRuntime());
}

void TVoxelMessageArgProcessor<FVoxelCallstack>::ProcessArg(FVoxelMessageBuilder& Builder, const FVoxelCallstack& Callstack)
{
	FVoxelGraphNodeRef LastNode;
	TArray<FVoxelGraphNodeRef> Nodes;
	for (const FVoxelCallstack* It = &Callstack; It; It = It->Parent.Get())
	{
		if (It->Node.IsExplicitlyNull() ||
			LastNode == It->Node)
		{
			continue;
		}
		LastNode = It->Node;

		Nodes.Add(It->Node);
	}

	if (Nodes.Num() == 0)
	{
		FVoxelMessageArgProcessor::ProcessArg(Builder, "<empty callstack>");
		return;
	}

	FString Format;
	for (int32 Index = 0; Index < Nodes.Num(); Index++)
	{
		if (!Format.IsEmpty())
		{
			Format += " -> ";
		}
		Format += FString::Printf(TEXT("{%d}"), Index);
	}

	const TSharedRef<FVoxelMessageBuilder> ChildBuilder = MakeVoxelShared<FVoxelMessageBuilder>(Builder.Severity, Format);
	for (const FVoxelGraphNodeRef& Node : Nodes)
	{
		FVoxelMessageArgProcessor::ProcessArg(*ChildBuilder, Node);
	}

	FVoxelMessageArgProcessor::ProcessArg(Builder, ChildBuilder);
}

void TVoxelMessageArgProcessor<FVoxelGraphNodeRef>::ProcessArg(FVoxelMessageBuilder& Builder, const FVoxelGraphNodeRef* NodeRef)
{
	if (!NodeRef)
	{
		FVoxelMessageArgProcessor::ProcessArg(Builder, "<null>");
		return;
	}

	Builder.AddArg([NodeRef = *NodeRef](FTokenizedMessage& Message, TSet<const UEdGraph*>& OutGraphs)
	{
#if WITH_EDITOR
		if (!NodeRef.IsExplicitlyNull() &&
			NodeRef.EdGraphNodeName_EditorOnly != FVoxelNodeNames::Builtin)
		{
			const UEdGraphNode* GraphNode = NodeRef.GetGraphNode_EditorOnly();
			if (ensureVoxelSlow(GraphNode))
			{
				FVoxelMessageBuilder LocalBuilder({}, {});
				FVoxelMessageArgProcessor::ProcessArg(LocalBuilder, GraphNode);
				LocalBuilder.Execute(Message, OutGraphs);
				return;
			}
		}
#endif

		FVoxelMessageBuilder LocalBuilder({}, {});
		FVoxelMessageArgProcessor::ProcessArg(LocalBuilder, NodeRef.DebugName);
		LocalBuilder.Execute(Message, OutGraphs);
	});
}

void TVoxelMessageArgProcessor<FVoxelGraphPinRef>::ProcessArg(FVoxelMessageBuilder& Builder, const FVoxelGraphPinRef* PinRef)
{
	if (!PinRef)
	{
		FVoxelMessageArgProcessor::ProcessArg(Builder, "<null>");
		return;
	}

	const TSharedRef<FVoxelMessageBuilder> ChildBuilder = MakeVoxelShared<FVoxelMessageBuilder>(Builder.Severity, "{0}.{1}");
	FVoxelMessageArgProcessor::ProcessArg(*ChildBuilder, PinRef->Node);
	FVoxelMessageArgProcessor::ProcessArg(*ChildBuilder, PinRef->PinName);
	FVoxelMessageArgProcessor::ProcessArg(Builder, ChildBuilder);
}

void TVoxelMessageArgProcessor<FVoxelCompiledNode>::ProcessArg(FVoxelMessageBuilder& Builder, const FVoxelCompiledNode* Node)
{
	FVoxelMessageArgProcessor::ProcessArg(Builder, Node ? &Node->Ref : nullptr);
}

void TVoxelMessageArgProcessor<Voxel::Graph::FPin>::ProcessArg(FVoxelMessageBuilder& Builder, const Voxel::Graph::FPin* Pin)
{
	VOXEL_USE_NAMESPACE(Graph);

	if (!ensure(Pin))
	{
		FVoxelMessageArgProcessor::ProcessArg(Builder, "Null");
		return;
	}

	const TSharedRef<FVoxelMessageBuilder> ChildBuilder = MakeVoxelShared<FVoxelMessageBuilder>(Builder.Severity, "{0}.{1}");
	FVoxelMessageArgProcessor::ProcessArg(*ChildBuilder, Pin->Node);

	FString PinName = Pin->Name.ToString();
#if WITH_EDITOR
	if (Pin->Node.Type == ENodeType::Struct)
	{
		if (const TSharedPtr<const FVoxelPin> VoxelPin = Pin->Node.GetVoxelNode().FindPin(Pin->Name))
		{
			PinName = VoxelPin->Metadata.DisplayName;
		}
	}
#endif
	FVoxelMessageArgProcessor::ProcessArg(*ChildBuilder, PinName);

	FVoxelMessageArgProcessor::ProcessArg(Builder, ChildBuilder);
}

void TVoxelMessageArgProcessor<Voxel::Graph::FNode>::ProcessArg(FVoxelMessageBuilder& Builder, const Voxel::Graph::FNode* Node)
{
	VOXEL_USE_NAMESPACE(Graph);

	if (!ensure(Node))
	{
		FVoxelMessageArgProcessor::ProcessArg(Builder, "Null");
		return;
	}

	FVoxelMessageArgProcessor::ProcessArg(Builder, Node->NodeRef);
}