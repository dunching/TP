// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphCompilerUtilities.h"
#include "VoxelGraph.h"
#include "VoxelGraphMessages.h"
#include "VoxelRootNode.h"
#include "VoxelExecNode.h"
#include "VoxelExecNodes.h"
#include "VoxelSpawnable.h"
#include "VoxelTemplateNode.h"
#include "VoxelParameterNode.h"
#include "VoxelFunctionCallNode.h"
#include "VoxelMergeSpawnableNode.h"
#include "Point/VoxelMergePointsNode.h"

BEGIN_VOXEL_NAMESPACE(Graph)

FCompilationScope* GCompilationScope = nullptr;

FCompilationScope::FCompilationScope()
{
	if (!GCompilationScope)
	{
		GCompilationScope = this;
	}
}

FCompilationScope::~FCompilationScope()
{
	if (GCompilationScope == this)
	{
		GCompilationScope = nullptr;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
TFunction<void(UVoxelGraph&)> FCompilerUtilities::ForceTranslateGraph_EditorOnly;
TFunction<void(UVoxelGraph&)> FCompilerUtilities::RaiseOrphanWarnings_EditorOnly;
#endif

bool FCompilerUtilities::IsNodeDeleted(const FVoxelGraphNodeRef& NodeRef)
{
	if (NodeRef.NodeId == FVoxelNodeNames::ExecuteNodeId)
	{
		return false;
	}

	const UVoxelGraph* Graph = NodeRef.GetGraph();
	if (!ensure(Graph))
	{
		return false;
	}

	return !Graph->CompiledGraph.Nodes.Contains(NodeRef.NodeId);
}

void FCompilerUtilities::TranslateIfNeeded(const UVoxelGraph& Graph)
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	if (!GEditor ||
		!ensure(ForceTranslateGraph_EditorOnly))
	{
		return;
	}

	const TSharedRef<FVoxelMessageSinkConsumer> MessageConsumer = MakeVoxelShared<FVoxelMessageSinkConsumer>();
	const FVoxelScopedMessageConsumer ScopedMessageConsumer = FVoxelScopedMessageConsumer(MessageConsumer);

	// Hack disable messages
	FVoxelGraphMessages* Messages = GVoxelGraphMessages;
	GVoxelGraphMessages = nullptr;
	ON_SCOPE_EXIT
	{
		GVoxelGraphMessages = Messages;
	};

	if (TranslateCompiledGraph(Graph))
	{
		// Nodes are up to date
		return;
	}

	if (GCompilationScope)
	{
		// Invalidate cache
		GCompilationScope->CacheMap.Remove(&Graph);
	}

	ForceTranslateGraph_EditorOnly(ConstCast(Graph));
#endif
}

TSharedPtr<FVoxelGraphExecutor> FCompilerUtilities::CreateExecutor(const FVoxelGraphPinRef& GraphPinRef)
{
	VOXEL_SCOPE_COUNTER_FORMAT("FCompilerUtilities::CreateExecutor %s", *GraphPinRef.ToString());
	check(IsInGameThread());
	const FCompilationScope CompilationScope;

#define RUN_PASS(Name, ...) \
	Name(*Graph, ##__VA_ARGS__); \
	if (Messages.HasError()) \
	{ \
		return nullptr;  \
	} \
	if (VOXEL_DEBUG) \
	{ \
		Graph->Check(); \
	}

	const UVoxelGraph* VoxelGraph = GraphPinRef.Node.GetGraph();
	if (!VoxelGraph)
	{
		// Don't return nullptr - this isn't a compilation error, just an empty graph

		const TSharedRef<FVoxelGraphExecutorInfo> GraphExecutorInfo = MakeVoxelShared<FVoxelGraphExecutorInfo>();
#if WITH_EDITOR
		GraphExecutorInfo->Graph_EditorOnly = MakeVoxelShared<Voxel::Graph::FGraph>();
#endif

		return MakeVoxelShared<FVoxelGraphExecutor>(
			nullptr,
			GraphExecutorInfo,
			TVoxelAddOnlySet<TSharedPtr<const FVoxelNode>>());
	}

	TranslateIfNeeded(*VoxelGraph);

	const FVoxelGraphMessages Messages(
#if WITH_EDITOR
		VoxelGraph->MainEdGraph
#else
		nullptr
#endif
	);

	TSharedPtr<FGraph> Graph;
	FNode* RootNode = nullptr;
	TSet<FObjectKey> OutReferencedObjects;
	TSet<TWeakObjectPtr<const UVoxelGraphInterface>> OutReferencedGraphs;
	{
		const TSharedPtr<const FGraph> ConstGraph = TranslateCompiledGraph(
			*VoxelGraph,
			&OutReferencedObjects,
			&OutReferencedGraphs);

		if (Messages.HasError() ||
			!ensure(ConstGraph))
		{
			return nullptr;
		}

		Graph = ConstGraph->Clone();

		FNode* Node = nullptr;
		for (FNode& NodeIt : Graph->GetNodes())
		{
			if (NodeIt.NodeRef.NodeId != GraphPinRef.Node.NodeId)
			{
				continue;
			}

			ensure(!Node);
			Node = &NodeIt;
		}

		if (!ensure(Node))
		{
			return nullptr;
		}

		FPin* Pin = Node->FindInput(GraphPinRef.PinName);
		if (!Pin)
		{
			// Array pin that was removed
			return nullptr;
		}

		if (Pin->Type.IsWildcard())
		{
			VOXEL_MESSAGE(Error, "Wildcard pin {0} needs to be converted. Please connect it to another pin or right click it -> Convert", Pin);
			return nullptr;
		}

		RootNode = &Graph->NewNode(ENodeType::Root, Node->NodeRef);

		Pin->CopyInputPinTo(RootNode->NewInputPin("Value", Pin->Type));

		Graph->RemoveNode(*Node);
	}

	RUN_PASS(RemoveUnusedNodes);
	RUN_PASS(CheckWildcards);
	RUN_PASS(CheckNoDefault);
	Graph->Check();

	// Checks for loops
	SortNodes(ReinterpretCastArray<const FNode*>(Graph->GetNodesArray()));
	if (Messages.HasError())
	{
		return nullptr;
	}

	RUN_PASS(RemovePassthroughs);
	RUN_PASS(DisconnectVirtualPins);
	RUN_PASS(RemoveUnusedNodes);

#undef RUN_PASS

	FNode& RuntimeRootNode = Graph->NewNode(ENodeType::Struct, RootNode->NodeRef);
	{
		const FVoxelPinType Type = RootNode->GetInputPin(0).Type;
		{
			const TSharedRef<FVoxelRootNode> RootVoxelNode = MakeVoxelShared<FVoxelRootNode>();
			RootVoxelNode->GetPin(RootVoxelNode->ValuePin).SetType(Type);
			RuntimeRootNode.SetVoxelNode(RootVoxelNode);
		}

		RootNode->GetInputPin(0).CopyInputPinTo(RuntimeRootNode.NewInputPin("Value", Type));
		Graph->RemoveNode(*RootNode);
	}

	for (const FNode& Node : Graph->GetNodes())
	{
		if (!ensure(Node.Type == ENodeType::Struct))
		{
			VOXEL_MESSAGE(Error, "INTERNAL ERROR: Unexpected node {0}", Node);
			return nullptr;
		}
	}

	TMap<const FNode*, TSharedPtr<FVoxelNode>> Nodes;
	{
		VOXEL_SCOPE_COUNTER("Copy nodes");
		for (FNode& Node : Graph->GetNodes())
		{
			Nodes.Add(&Node, Node.GetVoxelNode().MakeSharedCopy());
		}
	}

	for (const auto& It : Nodes)
	{
		It.Value->InitializeNodeRuntime(It.Key->NodeRef, false);

		if (Messages.HasError())
		{
			return nullptr;
		}
	}

	for (const auto& It : Nodes)
	{
		const FNode& Node = *It.Key;
		FVoxelNode& VoxelNode = *It.Value;

		TArray<TSharedPtr<FVoxelPin>> Pins;
		VoxelNode.GetPinsMap().GenerateValueArray(Pins);

		ensure(Pins.Num() == Node.GetPins().Num());

		// Assign compute to input pins
		for (const TSharedPtr<FVoxelPin>& Pin : Pins)
		{
			if (!Pin->bIsInput ||
				Pin->Metadata.bVirtualPin)
			{
				continue;
			}

			const FPin& InputPin = Node.FindInputChecked(Pin->Name);
			if (!ensure(InputPin.Type == Pin->GetType()))
			{
				VOXEL_MESSAGE(Error, "{0}: Internal error", InputPin);
				return nullptr;
			}

			const FVoxelNodeRuntime::FPinData* OutputPinData = nullptr;
			if (InputPin.GetLinkedTo().Num() > 0)
			{
				check(InputPin.GetLinkedTo().Num() == 1);
				const FPin& OutputPin = InputPin.GetLinkedTo()[0];
				check(OutputPin.Direction == EPinDirection::Output);

				OutputPinData = &Nodes[&OutputPin.Node]->GetNodeRuntime().GetPinData(OutputPin.Name);
				check(!OutputPinData->bIsInput);
			}

			FVoxelNodeRuntime::FPinData& PinData = ConstCast(VoxelNode.GetNodeRuntime().GetPinData(Pin->Name));
			if (!ensure(PinData.Type == InputPin.Type))
			{
				VOXEL_MESSAGE(Error, "{0}: Internal error", InputPin);
				return nullptr;
			}

			if (OutputPinData)
			{
				PinData.Compute = OutputPinData->Compute;
			}
			else if (InputPin.Type.HasPinDefaultValue())
			{
				const FVoxelRuntimePinValue DefaultValue = FVoxelPinType::MakeRuntimeValueFromInnerValue(
					InputPin.Type,
					InputPin.GetDefaultValue());

				if (!ensureVoxelSlow(DefaultValue.IsValid()))
				{
					VOXEL_MESSAGE(Error, "{0}: Invalid default value", InputPin);
					return nullptr;
				}

				ensure(DefaultValue.GetType().CanBeCastedTo(InputPin.Type));

				if (VOXEL_DEBUG)
				{
					const FString DefaultValueString = InputPin.GetDefaultValue().ExportToString();

					FVoxelPinValue TestValue(InputPin.GetDefaultValue().GetType());
					ensure(TestValue.ImportFromString(DefaultValueString));
					ensure(TestValue == InputPin.GetDefaultValue());
				}
				if (VOXEL_DEBUG)
				{
					const FVoxelPinValue ExposedValue = FVoxelPinType::MakeExposedInnerValue(DefaultValue);
					ensure(ExposedValue == InputPin.GetDefaultValue());
				}

				PinData.Compute = MakeVoxelShared<FVoxelComputeValue>([Value = FVoxelFutureValue(DefaultValue)](const FVoxelQuery&)
				{
					return Value;
				});
			}
			else
			{
				FVoxelRuntimePinValue Value(InputPin.Type);
				if (InputPin.Type.IsBufferArray())
				{
					// We don't want to have the default element
					ConstCast(Value.Get<FVoxelBuffer>()).SetAsEmpty();
				}

				PinData.Compute = MakeVoxelShared<FVoxelComputeValue>([FutureValue = FVoxelFutureValue(Value)](const FVoxelQuery&)
				{
					return FutureValue;
				});
			}
		}
	}

	for (const auto& It : Nodes)
	{
		It.Value->RemoveEditorData();
		It.Value->EnableSharedNode(It.Value.ToSharedRef());
	}

	TVoxelAddOnlySet<TSharedPtr<const FVoxelNode>> ExecutorNodes;
	for (const auto& It : Nodes)
	{
		ExecutorNodes.Add(It.Value);
	}

	const TSharedRef<FVoxelGraphExecutorInfo> GraphExecutorInfo = MakeVoxelShared<FVoxelGraphExecutorInfo>();
#if WITH_EDITOR
	GraphExecutorInfo->Graph_EditorOnly = Graph->Clone();
#endif
	GraphExecutorInfo->ReferencedObjects = MoveTemp(OutReferencedObjects);
	GraphExecutorInfo->ReferencedGraphs = MoveTemp(OutReferencedGraphs);

	return MakeVoxelShared<FVoxelGraphExecutor>(
		CastChecked<FVoxelRootNode>(Nodes[&RuntimeRootNode].ToSharedRef()),
		GraphExecutorInfo,
		MoveTemp(ExecutorNodes));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<const FGraph> FCompilerUtilities::TranslateCompiledGraph(
	const UVoxelGraph& VoxelGraph,
	TSet<FObjectKey>* OutReferencedObjects,
	TSet<TWeakObjectPtr<const UVoxelGraphInterface>>* OutReferencedGraphs)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());
	const FCompilationScope CompilationScope;

	TSharedPtr<FCompilationScope::FCache>& Cache = GCompilationScope->CacheMap.FindOrAdd(&VoxelGraph);
	if (!Cache)
	{
		Cache = MakeVoxelShared<FCompilationScope::FCache>();

		const TSharedPtr<FGraph> Graph = TranslateCompiledGraphImpl(
			VoxelGraph,
			Cache->ReferencedObjects,
			Cache->ReferencedGraphs);

		if (Graph)
		{
			AddExecOutput(*Graph, VoxelGraph);
			ReplaceTemplates(*Graph);
		}

		Cache->Graph = Graph;
	}

	if (OutReferencedObjects)
	{
		*OutReferencedObjects = Cache->ReferencedObjects;
	}
	if (OutReferencedGraphs)
	{
		*OutReferencedGraphs = Cache->ReferencedGraphs;
	}

	return Cache->Graph;
}

TSharedPtr<FGraph> FCompilerUtilities::TranslateCompiledGraphImpl(
	const UVoxelGraph& VoxelGraph,
	TSet<FObjectKey>& OutReferencedObjects,
	TSet<TWeakObjectPtr<const UVoxelGraphInterface>>& OutReferencedGraphs)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	const TArray<FVoxelGraphParameter>& Parameters = VoxelGraph.Parameters;
	const FVoxelCompiledGraph& CompiledGraph = VoxelGraph.CompiledGraph;

	if (!CompiledGraph.bIsValid)
	{
		VOXEL_MESSAGE(Error, "{0}: graph failed to translate", VoxelGraph);
		return nullptr;
	}

	const TSharedRef<FGraph> Graph = MakeVoxelShared<FGraph>();

	TVoxelMap<const FVoxelCompiledNode*, FNode*> NodesMap;
	TVoxelSet<const FVoxelCompiledNode*> SkippedNodes;

	for (const auto& CompiledNodeIt : CompiledGraph.Nodes)
	{
		const FVoxelCompiledNode& CompiledNode = CompiledNodeIt.Value;
		if (!ensureVoxelSlow(CompiledNode.Ref.Graph.IsValid()) ||
			!ensureVoxelSlow(!CompiledNode.Ref.NodeId.IsNone()))
		{
#if WITH_EDITOR
			if (CompiledNode.Struct)
			{
				ensureVoxelSlow(!GVoxelGraphMessages);
				VOXEL_MESSAGE(Error, "{0}: Outdated node {1}", VoxelGraph, CompiledNode.Struct->GetDisplayName());
			}
			else
#endif
			{
				ensureVoxelSlow(!GVoxelGraphMessages);
				VOXEL_MESSAGE(Error, "{0}: Outdated node", VoxelGraph);
			}
			return nullptr;
		}
		VOXEL_SCOPE_COUNTER_FNAME(CompiledNode.Ref.DebugName);

		FNode* Node = nullptr;
		switch (CompiledNode.Type)
		{
		default:
		{
			VOXEL_MESSAGE(Error, "Invalid node {0}", CompiledNode);
			return nullptr;
		}
		case EVoxelCompiledNodeType::Struct:
		{
			if (!CompiledNode.Struct.IsValid())
			{
				for (const auto& It : CompiledNode.InputPins)
				{
					if (It.Value.LinkedTo.Num() > 0)
					{
						VOXEL_MESSAGE(Error, "Invalid struct on {0}", CompiledNode);
						return nullptr;
					}
				}
				for (const auto& It : CompiledNode.OutputPins)
				{
					if (It.Value.LinkedTo.Num() > 0)
					{
						VOXEL_MESSAGE(Error, "Invalid struct on {0}", CompiledNode);
						return nullptr;
					}
				}

				// This node isn't connected to anything, just skip it
				VOXEL_MESSAGE(Warning, "Invalid struct on {0}", CompiledNode);
				SkippedNodes.Add(&CompiledNode);
				continue;
			}

			Node = &Graph->NewNode(ENodeType::Struct, CompiledNode.Ref);

			VOXEL_SCOPE_COUNTER_FORMAT("Copy node %s", *CompiledNode.Struct->GetStruct()->GetName());
			Node->SetVoxelNode(CompiledNode.Struct->MakeSharedCopy());
		}
		break;
		case EVoxelCompiledNodeType::Macro:
		{
			if (!CompiledNode.Graph)
			{
				if (CompiledNode.InputPins.Num() > 0 ||
					CompiledNode.OutputPins.Num() > 0)
				{
					VOXEL_MESSAGE(Error, "{0}: Graph is null", CompiledNode);
					return nullptr;
				}

				SkippedNodes.Add(&CompiledNode);
				continue;
			}

			OutReferencedGraphs.Add(CompiledNode.Graph);

			Node = &Graph->NewNode(ENodeType::Struct, CompiledNode.Ref);

			const bool bHasTemplatePin = CompiledNode.InputPins.Find(FVoxelNodeNames::MacroTemplateInput) != nullptr;
			const bool bHasRecursiveTemplatePin = CompiledNode.InputPins.Find(FVoxelNodeNames::MacroRecursiveTemplateInput) != nullptr;
			ensure(!bHasTemplatePin || !bHasRecursiveTemplatePin);

			const FVoxelNode_FunctionCall::EMode Mode =
				bHasTemplatePin
				? FVoxelNode_FunctionCall::EMode::Template
				: bHasRecursiveTemplatePin
				? FVoxelNode_FunctionCall::EMode::RecursiveTemplate
				: FVoxelNode_FunctionCall::EMode::Macro;

			const TSharedRef<FVoxelNode_FunctionCall> FunctionCallNode = MakeVoxelShared<FVoxelNode_FunctionCall>();
			FunctionCallNode->Initialize(Mode, *CompiledNode.Graph);
			Node->SetVoxelNode(FunctionCallNode);
		}
		break;
		case EVoxelCompiledNodeType::Parameter:
		{
			const FVoxelGraphParameter* Parameter = Parameters.FindByKey(CompiledNode.Guid);
			if (!Parameter ||
				Parameter->ParameterType != EVoxelGraphParameterType::Parameter ||
				!Parameter->Type.IsValid() ||
				!ensure(CompiledNode.InputPins.Num() == 0) ||
				!ensure(CompiledNode.OutputPins.Num() == 1) ||
				!ensure(CompiledNode.OutputPins.Contains("Value")) ||
				CompiledNode.OutputPins["Value"].Type != Parameter->Type)
			{
				ensureVoxelSlow(!GVoxelGraphMessages);
				VOXEL_MESSAGE(Error, "Invalid parameter {0}", CompiledNode);
				return nullptr;
			}

			Node = &Graph->NewNode(ENodeType::Struct, CompiledNode.Ref);

			const TSharedRef<FVoxelNode_Parameter> ParameterNode = MakeVoxelShared<FVoxelNode_Parameter>();
			ParameterNode->ParameterGuid = Parameter->Guid;
			ParameterNode->ParameterName = Parameter->Name;
			ParameterNode->GetPin(ParameterNode->ValuePin).SetType(Parameter->Type);
			Node->SetVoxelNode(ParameterNode);
		}
		break;
		case EVoxelCompiledNodeType::Input:
		{
			const FVoxelGraphParameter* Input = Parameters.FindByKey(CompiledNode.Guid);
			if (!Input ||
				Input->ParameterType != EVoxelGraphParameterType::Input ||
				!Input->Type.IsValid() ||
				!ensureVoxelSlow(CompiledNode.OutputPins.Num() == 1) ||
				!ensureVoxelSlow(CompiledNode.OutputPins.Contains("Value")) ||
				CompiledNode.OutputPins["Value"].Type != Input->Type)
			{
				ensureVoxelSlow(!GVoxelGraphMessages);
				VOXEL_MESSAGE(Error, "Invalid input {0}", CompiledNode);
				return nullptr;
			}

			if (Input->bExposeInputDefaultAsPin)
			{
				if (!ensureVoxelSlow(CompiledNode.InputPins.Num() == 1) ||
					!ensureVoxelSlow(CompiledNode.InputPins.Contains("Default")) ||
					CompiledNode.InputPins["Default"].Type != Input->Type)
				{
					ensureVoxelSlow(!GVoxelGraphMessages);
					VOXEL_MESSAGE(Error, "Invalid input {0}", CompiledNode);
					return nullptr;
				}

				Node = &Graph->NewNode(ENodeType::Struct, CompiledNode.Ref);

				const TSharedRef<FVoxelNode_FunctionCallInput_WithDefaultPin> InputNode = MakeVoxelShared<FVoxelNode_FunctionCallInput_WithDefaultPin>();
				InputNode->Name = Input->Name;
				InputNode->GetPin(InputNode->DefaultPin).SetType(Input->Type);
				InputNode->GetPin(InputNode->ValuePin).SetType(Input->Type);
				Node->SetVoxelNode(InputNode);
			}
			else
			{
				if (!ensureVoxelSlow(CompiledNode.InputPins.Num() == 0))
				{
					ensureVoxelSlow(!GVoxelGraphMessages);
					VOXEL_MESSAGE(Error, "Invalid input {0}", CompiledNode);
					return nullptr;
				}

				Node = &Graph->NewNode(ENodeType::Struct, CompiledNode.Ref);

				const TSharedRef<FVoxelNode_FunctionCallInput_WithoutDefaultPin> InputNode = MakeVoxelShared<FVoxelNode_FunctionCallInput_WithoutDefaultPin>();
				InputNode->Name = Input->Name;
				InputNode->DefaultValue = FVoxelPinType::MakeRuntimeValueFromInnerValue(Input->Type, Input->DefaultValue);
				InputNode->GetPin(InputNode->ValuePin).SetType(Input->Type);
				Node->SetVoxelNode(InputNode);
			}
		}
		break;
		case EVoxelCompiledNodeType::Output:
		{
			const FVoxelGraphParameter* Output = Parameters.FindByKey(CompiledNode.Guid);
			if (!Output ||
				Output->ParameterType != EVoxelGraphParameterType::Output ||
				!Output->Type.IsValid() ||
				!ensureVoxelSlow(CompiledNode.InputPins.Num() == 1) ||
				!ensureVoxelSlow(CompiledNode.OutputPins.Num() == 0) ||
				!ensureVoxelSlow(CompiledNode.InputPins.Contains("Value")) ||
				CompiledNode.InputPins["Value"].Type != Output->Type ||
				CompiledNode.Ref.NodeId != FName("Output." + Output->Guid.ToString()))
			{
				ensureVoxelSlow(!GVoxelGraphMessages);
				VOXEL_MESSAGE(Error, "Invalid output {0}", CompiledNode);
				return nullptr;
			}

			Node = &Graph->NewNode(ENodeType::Struct, CompiledNode.Ref);

			const TSharedRef<FVoxelNode_FunctionCallOutput> OutputNode = MakeVoxelShared<FVoxelNode_FunctionCallOutput>();
			OutputNode->GetPin(OutputNode->ValuePin).SetType(Output->Type);
			Node->SetVoxelNode(OutputNode);
		}
		break;
		}
		check(Node);

		if (Node->Type == ENodeType::Struct)
		{
			const FVoxelNode& VoxelNode = Node->GetVoxelNode();
			for (const FVoxelPin& Pin : VoxelNode.GetPins())
			{
				if (!(Pin.bIsInput ? CompiledNode.InputPins : CompiledNode.OutputPins).Contains(Pin.Name))
				{
					ensureVoxelSlow(!GVoxelGraphMessages);
					VOXEL_MESSAGE(Error, "Outdated node {0}", CompiledNode);
					return nullptr;
				}
			}
			for (const auto& CompiledPinIt : CompiledNode.InputPins)
			{
				const FVoxelCompiledPin& CompiledPin = CompiledPinIt.Value;
				const TSharedPtr<const FVoxelPin> Pin = VoxelNode.FindPin(CompiledPin.PinName);
				if (!Pin ||
					!Pin->bIsInput)
				{
					ensureVoxelSlow(!GVoxelGraphMessages);
					VOXEL_MESSAGE(Error, "Outdated node {0}", CompiledNode);
					return nullptr;
				}

				if (CompiledPin.Type != Pin->GetType())
				{
					ensureVoxelSlow(!GVoxelGraphMessages);
					VOXEL_MESSAGE(Error, "Outdated node {0}", CompiledNode);
					return nullptr;
				}
			}
			for (const auto& CompiledPinIt : CompiledNode.OutputPins)
			{
				const FVoxelCompiledPin& CompiledPin = CompiledPinIt.Value;
				const TSharedPtr<const FVoxelPin> Pin = VoxelNode.FindPin(CompiledPin.PinName);
				if (!Pin ||
					Pin->bIsInput)
				{
					ensureVoxelSlow(!GVoxelGraphMessages);
					VOXEL_MESSAGE(Error, "Outdated node {0}", CompiledNode);
					return nullptr;
				}

				if (CompiledPin.Type != Pin->GetType())
				{
					ensureVoxelSlow(!GVoxelGraphMessages);
					VOXEL_MESSAGE(Error, "Outdated node {0}", CompiledNode);
					return nullptr;
				}
			}
		}

		NodesMap.Add(&CompiledNode, Node);

		for (const auto& CompiledPinIt : CompiledNode.InputPins)
		{
			const FVoxelCompiledPin& CompiledPin = CompiledPinIt.Value;
			if (!CompiledPin.Type.IsValid())
			{
				VOXEL_MESSAGE(Error, "Invalid pin on {0}", CompiledNode);
				return nullptr;
			}

			FVoxelPinValue DefaultValue;
			if (CompiledPin.Type.HasPinDefaultValue())
			{
				DefaultValue = CompiledPin.DefaultValue;

				if (!DefaultValue.IsValid() ||
					!ensureVoxelSlow(DefaultValue.GetType().CanBeCastedTo(CompiledPin.Type.GetPinDefaultValueType())))
				{
					VOXEL_MESSAGE(Error, "{0}: Invalid default value", &CompiledNode);
					return nullptr;
				}

				if (DefaultValue.IsObject())
				{
					if (UObject* Object = DefaultValue.GetObject())
					{
						OutReferencedObjects.Add(Object);
					}
				}
			}
			else
			{
				ensureVoxelSlow(!CompiledPin.DefaultValue.IsValid());
			}

			Node->NewInputPin(CompiledPin.PinName, CompiledPin.Type, DefaultValue);
		}

		for (const auto& CompiledPinIt : CompiledNode.OutputPins)
		{
			const FVoxelCompiledPin& CompiledPin = CompiledPinIt.Value;
			if (!CompiledPin.Type.IsValid())
			{
				VOXEL_MESSAGE(Error, "Invalid pin on {0}", CompiledNode);
				return nullptr;
			}

			Node->NewOutputPin(CompiledPin.PinName, CompiledPin.Type);
		}
	}

	VOXEL_SCOPE_COUNTER("Fixup links");

	// Fixup links after all nodes are created
	for (const auto& CompiledNodeIt : CompiledGraph.Nodes)
	{
		const FVoxelCompiledNode& CompiledNode = CompiledNodeIt.Value;
		if (SkippedNodes.Contains(&CompiledNode))
		{
			continue;
		}

		FNode& Node = *NodesMap[&CompiledNode];

		for (const auto& CompiledPinIt : CompiledNode.InputPins)
		{
			const FVoxelCompiledPin& InputPin = CompiledPinIt.Value;
			if (InputPin.LinkedTo.Num() > 1)
			{
				VOXEL_MESSAGE(Error, "Too many pins linked to {0}.{1}", CompiledNode, InputPin.PinName);
				return nullptr;
			}

			for (const FVoxelCompiledPinRef& OutputPinRef : InputPin.LinkedTo)
			{
				check(!OutputPinRef.bIsInput);

				const FVoxelCompiledPin* OutputPin = CompiledGraph.FindPin(OutputPinRef);
				if (!ensure(OutputPin))
				{
					VOXEL_MESSAGE(Error, "Invalid pin ref on {0}", CompiledNode);
					continue;
				}

				const FVoxelCompiledNode& OtherCompiledNode = CompiledGraph.Nodes[OutputPinRef.NodeId];
				FNode& OtherNode = *NodesMap[&OtherCompiledNode];

				if (!OutputPin->Type.CanBeCastedTo(InputPin.Type))
				{
					VOXEL_MESSAGE(Error, "Invalid pin link from {0}.{1} to {2}.{3}: type mismatch: {4} vs {5}",
						CompiledNode,
						OutputPin->PinName,
						OtherCompiledNode,
						InputPin.PinName,
						OutputPin->Type.ToString(),
						InputPin.Type.ToString());

					continue;
				}

				Node.FindInputChecked(InputPin.PinName).MakeLinkTo(OtherNode.FindOutputChecked(OutputPinRef.PinName));
			}
		}
	}

	// Input links are used to populate all links, check they're correct with output links
	for (const auto& CompiledNodeIt : CompiledGraph.Nodes)
	{
		const FVoxelCompiledNode& CompiledNode = CompiledNodeIt.Value;
		if (SkippedNodes.Contains(&CompiledNode))
		{
			continue;
		}

		FNode& Node = *NodesMap[&CompiledNode];

		for (const auto& CompiledPinIt : CompiledNode.OutputPins)
		{
			const FVoxelCompiledPin& OutputPin = CompiledPinIt.Value;
			for (const FVoxelCompiledPinRef& InputPinRef : OutputPin.LinkedTo)
			{
				check(InputPinRef.bIsInput);

				const FVoxelCompiledPin* InputPin = CompiledGraph.FindPin(InputPinRef);
				if (!ensure(InputPin))
				{
					VOXEL_MESSAGE(Error, "Invalid pin ref on {0}", CompiledNode);
					continue;
				}

				const FVoxelCompiledNode& OtherCompiledNode = CompiledGraph.Nodes[InputPinRef.NodeId];
				FNode& OtherNode = *NodesMap[&OtherCompiledNode];

				if (!OutputPin.Type.CanBeCastedTo(InputPin->Type))
				{
					VOXEL_MESSAGE(Error, "Invalid pin link from {0}.{1} to {2}.{3}: type mismatch: {4} vs {5}",
						CompiledNode,
						OutputPin.PinName,
						OtherCompiledNode,
						InputPin->PinName,
						OutputPin.Type.ToString(),
						InputPin->Type.ToString());

					continue;
				}

				if (!ensure(Node.FindOutputChecked(OutputPin.PinName).IsLinkedTo(OtherNode.FindInputChecked(InputPinRef.PinName))))
				{
					VOXEL_MESSAGE(Error, "Translation error: {0} -> {1}", Node, OtherNode);
				}
			}
		}
	}

	Graph->Check();
	return Graph;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FCompilerUtilities::AddExecOutput(FGraph& Graph, const UVoxelGraph& VoxelGraph)
{
	VOXEL_FUNCTION_COUNTER();

	FNode& MergeNode = Graph.NewNode(ENodeType::Struct, FVoxelGraphNodeRef
	{
		&VoxelGraph,
		FVoxelNodeNames::MergeNodeId
	});

	{
		const TSharedRef<FVoxelNode_MergeExecs> MergeVoxelNode = MakeVoxelShared<FVoxelNode_MergeExecs>();
		MergeNode.NewInputPin("Execs_0", FVoxelPinType::Make<FVoxelExec>(), FVoxelPinValue::Make<FVoxelExec>());
		MergeNode.NewOutputPin(VOXEL_PIN_NAME(FVoxelNode_MergeExecs, ExecPin), FVoxelPinType::Make<FVoxelExec>());
		MergeNode.SetVoxelNode(MergeVoxelNode);

		for (FNode& Node : Graph.GetNodesCopy())
		{
			if (!Node.GetVoxelNode().IsA<FVoxelNode_Execute>())
			{
				continue;
			}

			const FName Name = MergeVoxelNode->AddPinToArray(VOXEL_PIN_NAME(FVoxelNode_MergeExecs, ExecsPins));
			FPin& Pin = MergeNode.NewInputPin(Name, FVoxelPinType::Make<FVoxelExec>());
			Node.FindInputChecked(VOXEL_PIN_NAME(FVoxelNode_Execute, ExecPin)).CopyInputPinTo(Pin);

			Graph.RemoveNode(Node);
		}

		for (FNode& Node : Graph.GetNodesCopy())
		{
			if (!Node.GetVoxelNode().IsA<FVoxelExecNode>())
			{
				continue;
			}

			FPin& ExecPin = Node.FindOutputChecked(VOXEL_PIN_NAME(FVoxelExecNode, ExecPin));
			if (ExecPin.GetLinkedTo().Num() > 0)
			{
				continue;
			}

			const FName Name = MergeVoxelNode->AddPinToArray(VOXEL_PIN_NAME(FVoxelNode_MergeExecs, ExecsPins));
			FPin& Pin = MergeNode.NewInputPin(Name, FVoxelPinType::Make<FVoxelExec>());
			ExecPin.MakeLinkTo(Pin);
		}
	}

	// Add an execute node that the runtime will use to execute the graph
	// Make sure to do so after iterating all nodes to not auto execute this node

	FNode& ExecuteNode = Graph.NewNode(ENodeType::Struct, FVoxelGraphNodeRef
	{
		&VoxelGraph,
		FVoxelNodeNames::ExecuteNodeId
	});

	ExecuteNode.SetVoxelNode(MakeVoxelShared<FVoxelRootExecuteNode>());
	ExecuteNode.NewInputPin(VOXEL_PIN_NAME(FVoxelRootExecuteNode, ExecInPin), FVoxelPinType::Make<FVoxelExec>());
	ExecuteNode.NewInputPin(VOXEL_PIN_NAME(FVoxelRootExecuteNode, EnableNodePin), FVoxelPinType::Make<bool>(), FVoxelPinValue::Make(true));
	ExecuteNode.NewOutputPin(VOXEL_PIN_NAME(FVoxelRootExecuteNode, ExecPin), FVoxelPinType::Make<FVoxelExec>());
	ExecuteNode.GetInputPin(0).MakeLinkTo(MergeNode.GetOutputPin(0));
}

void FCompilerUtilities::CheckWildcards(const FGraph& Graph)
{
	VOXEL_FUNCTION_COUNTER();

	for (const FNode& Node : Graph.GetNodes())
	{
		for (const FPin& Pin : Node.GetPins())
		{
			if (Pin.Type.IsWildcard())
			{
				VOXEL_MESSAGE(Error, "Wildcard pin {0} needs to be converted. Please connect it to another pin or right click it -> Convert", Pin);
			}
		}
	}
}

void FCompilerUtilities::CheckNoDefault(const FGraph& Graph)
{
	VOXEL_FUNCTION_COUNTER();

	for (const FNode& Node : Graph.GetNodes())
	{
		if (Node.Type != ENodeType::Struct)
		{
			continue;
		}

		for (const FPin& Pin : Node.GetPins())
		{
			if (Pin.GetLinkedTo().Num() > 0)
			{
				continue;
			}

			const TSharedPtr<const FVoxelPin> VoxelPin = Node.GetVoxelNode().FindPin(Pin.Name);
			if (!ensure(VoxelPin) ||
				!VoxelPin->Metadata.bNoDefault)
			{
				continue;
			}

			VOXEL_MESSAGE(Error, "Pin {0} needs to be connected", Pin);
		}
	}
}

void FCompilerUtilities::ReplaceTemplates(FGraph& Graph)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	while (ReplaceTemplatesImpl(Graph))
	{
	}
}

bool FCompilerUtilities::ReplaceTemplatesImpl(FGraph& Graph)
{
	VOXEL_FUNCTION_COUNTER();

	for (FNode& Node : Graph.GetNodes())
	{
		if (Node.Type != ENodeType::Struct ||
			!Node.GetVoxelNode().IsA<FVoxelTemplateNode>())
		{
			continue;
		}

		bool bHasWildcardPin = false;
		for (const FPin& Pin : Node.GetPins())
		{
			if (Pin.Type.IsWildcard())
			{
				bHasWildcardPin = true;
			}
		}

		if (bHasWildcardPin)
		{
			// Can't replace template with a wildcard
			// If this node is unused, it'll be removed by RemoveUnusedNodes down the pipeline
			// If it is used, CheckWildcards will catch it
			continue;
		}

		FVoxelTemplateNodeContext Context(Node.NodeRef);

		ensure(!GVoxelTemplateNodeContext);
		GVoxelTemplateNodeContext = &Context;
		ON_SCOPE_EXIT
		{
			ensure(GVoxelTemplateNodeContext == &Context);
			GVoxelTemplateNodeContext = nullptr;
		};

		InitializeTemplatesPassthroughNodes(Graph, Node);

		Node.GetVoxelNode<FVoxelTemplateNode>().ExpandNode(Graph, Node);
		Graph.RemoveNode(Node);

		return true;
	}

	return false;
}

void FCompilerUtilities::InitializeTemplatesPassthroughNodes(FGraph& Graph, FNode& Node)
{
	for (FPin& InputPin : Node.GetInputPins())
	{
		FNode& Passthrough = Graph.NewNode(ENodeType::Passthrough, FVoxelTemplateNodeUtilities::GetNodeRef());
		FPin& PassthroughInputPin = Passthrough.NewInputPin("Input" + InputPin.Name, InputPin.Type);
		FPin& PassthroughOutputPin = Passthrough.NewOutputPin(InputPin.Name, InputPin.Type);

		InputPin.CopyInputPinTo(PassthroughInputPin);

		InputPin.BreakAllLinks();
		InputPin.MakeLinkTo(PassthroughOutputPin);
	}

	for (FPin& OutputPin : Node.GetOutputPins())
	{
		FNode& Passthrough = Graph.NewNode(ENodeType::Passthrough, FVoxelTemplateNodeUtilities::GetNodeRef());
		FPin& PassthroughInputPin = Passthrough.NewInputPin(OutputPin.Name, OutputPin.Type);
		if (OutputPin.Type.HasPinDefaultValue())
		{
			PassthroughInputPin.SetDefaultValue(FVoxelPinValue(OutputPin.Type.GetPinDefaultValueType()));
		}
		FPin& PassthroughOutputPin = Passthrough.NewOutputPin("Output" + OutputPin.Name, OutputPin.Type);

		OutputPin.CopyOutputPinTo(PassthroughOutputPin);

		OutputPin.BreakAllLinks();
		OutputPin.MakeLinkTo(PassthroughInputPin);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FCompilerUtilities::RemovePassthroughs(FGraph& Graph)
{
	VOXEL_FUNCTION_COUNTER();

	Graph.RemoveNodes([&](FNode& Node)
	{
		if (Node.Type != ENodeType::Passthrough)
		{
			return false;
		}

		const int32 Num = Node.GetInputPins().Num();
		check(Num == Node.GetOutputPins().Num());

		for (int32 Index = 0; Index < Num; Index++)
		{
			const FPin& InputPin = Node.GetInputPin(Index);
			const FPin& OutputPin = Node.GetOutputPin(Index);

			for (FPin& LinkedTo : OutputPin.GetLinkedTo())
			{
				InputPin.CopyInputPinTo(LinkedTo);
			}
		}

		return true;
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FCompilerUtilities::DisconnectVirtualPins(FGraph& Graph)
{
	VOXEL_FUNCTION_COUNTER();

	for (FNode& Node : Graph.GetNodes())
	{
		if (Node.Type != ENodeType::Struct)
		{
			continue;
		}

		for (FPin& Pin : Node.GetInputPins())
		{
			const TSharedPtr<const FVoxelPin> VoxelPin = Node.GetVoxelNode().FindPin(Pin.Name);
			if (!ensure(VoxelPin) ||
				!VoxelPin->Metadata.bVirtualPin)
			{
				continue;
			}
			ensure(Pin.Direction == EPinDirection::Input);

			Pin.BreakAllLinks();

			if (Pin.Type.HasPinDefaultValue())
			{
				Pin.SetDefaultValue(FVoxelPinValue(Pin.Type.GetPinDefaultValueType()));
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FCompilerUtilities::RemoveUnusedNodes(FGraph& Graph)
{
	VOXEL_FUNCTION_COUNTER();

	TArray<const FNode*> Nodes;
	for (const FNode& Node : Graph.GetNodes())
	{
		if (Node.Type == ENodeType::Root)
		{
			Nodes.Add(&Node);
		}
	}
	ensure(Nodes.Num() == 1);

	TSet<const FNode*> ValidNodes;

	TArray<const FNode*> NodesToVisit = Nodes;
	while (NodesToVisit.Num() > 0)
	{
		const FNode* Node = NodesToVisit.Pop(false);
		if (ValidNodes.Contains(Node))
		{
			continue;
		}
		ValidNodes.Add(Node);

		for (const FPin& Pin : Node->GetInputPins())
		{
			for (const FPin& LinkedTo : Pin.GetLinkedTo())
			{
				NodesToVisit.Add(&LinkedTo.Node);
			}
		}
	}

	Graph.RemoveNodes([&](const FNode& Node)
	{
		return !ValidNodes.Contains(&Node);
	});
}

TArray<const FNode*> FCompilerUtilities::SortNodes(const TArray<const FNode*>& Nodes)
{
	VOXEL_FUNCTION_COUNTER();

	TSet<const FNode*> VisitedNodes;
	TArray<const FNode*> SortedNodes;
	TArray<const FNode*> NodesToSort = Nodes;

	while (NodesToSort.Num() > 0)
	{
		bool bHasRemovedNode = false;
		for (int32 Index = 0; Index < NodesToSort.Num(); Index++)
		{
			const FNode* Node = NodesToSort[Index];

			const bool bHasInputPin = INLINE_LAMBDA
			{
				for (const FPin& Pin : Node->GetInputPins())
				{
					for (const FPin& LinkedTo : Pin.GetLinkedTo())
					{
						if (!VisitedNodes.Contains(&LinkedTo.Node))
						{
							return true;
						}
					}
				}
				return false;
			};

			if (bHasInputPin)
			{
				continue;
			}

			VisitedNodes.Add(Node);
			SortedNodes.Add(Node);

			NodesToSort.RemoveAtSwap(Index);
			Index--;
			bHasRemovedNode = true;
		}

		if (!bHasRemovedNode &&
			NodesToSort.Num() > 0)
		{
			VOXEL_MESSAGE(Error, "Loop in a graph {0}", NodesToSort);
			return {};
		}
	}

	return SortedNodes;
}

END_VOXEL_NAMESPACE(Graph)