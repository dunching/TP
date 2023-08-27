// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphToolkit.h"
#include "VoxelNode.h"
#include "VoxelEdGraph.h"
#include "VoxelGraph.h"
#include "VoxelGraphSchema.h"
#include "VoxelGraphSearchManager.h"
#include "Widgets/SVoxelGraphSearch.h"
#include "Widgets/SVoxelGraphMembers.h"
#include "Widgets/SVoxelGraphPreview.h"
#include "VoxelGraphCompilerUtilities.h"
#include "VoxelGraphEditorCompiler.h"
#include "Nodes/VoxelGraphKnotNode.h"
#include "Nodes/VoxelGraphStructNode.h"
#include "Nodes/VoxelGraphParameterNode.h"
#include "Nodes/VoxelGraphLocalVariableNode.h"
#include "Nodes/VoxelGraphMacroParameterNode.h"
#include "Customizations/VoxelGraphNodeCustomization.h"
#include "Customizations/VoxelGraphPreviewSettingsCustomization.h"
#include "Customizations/VoxelGraphParameterSelectionCustomization.h"

#include "SGraphPanel.h"
#include "MessageLogModule.h"
#include "IMessageLogListing.h"
#include "VoxelGraphMessages.h"

class FVoxelGraphCommands : public TVoxelCommands<FVoxelGraphCommands>
{
public:
	TSharedPtr<FUICommandInfo> EnableStats;
	TSharedPtr<FUICommandInfo> FindInGraph;
	TSharedPtr<FUICommandInfo> FindInGraphs;
	TSharedPtr<FUICommandInfo> ReconstructAllNodes;
	TSharedPtr<FUICommandInfo> TogglePreview;

	virtual void RegisterCommands() override;
};

DEFINE_VOXEL_COMMANDS(FVoxelGraphCommands);

void FVoxelGraphCommands::RegisterCommands()
{
	VOXEL_UI_COMMAND(EnableStats, "Enable Stats", "Enable stats)", EUserInterfaceActionType::ToggleButton, FInputChord());
	VOXEL_UI_COMMAND(FindInGraph, "Find", "Finds references to functions, variables, and pins in the current Graph (use Ctrl+Shift+F to search in all Graphs)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::F));
	VOXEL_UI_COMMAND(FindInGraphs, "Find in Graphs", "Find references to functions and variables in ALL Graphs", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::F));
	VOXEL_UI_COMMAND(ReconstructAllNodes, "Reconstruct all nodes", "Reconstructs all nodes in the graph", EUserInterfaceActionType::Button, FInputChord());
	VOXEL_UI_COMMAND(TogglePreview, "Toggle Preview", "Toggle node preview", EUserInterfaceActionType::Button, FInputChord(EKeys::D));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TObjectPtr<UEdGraph> FVoxelGraphToolkit::CreateGraph(UVoxelGraph* Owner)
{
	UVoxelEdGraph* Graph = NewObject<UVoxelEdGraph>(Owner, NAME_None, RF_Transactional);
	Graph->Schema = UVoxelGraphSchema::StaticClass();
	Graph->WeakToolkit = SharedThis(this);
	return Graph;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGraphToolkit::Initialize()
{
	Super::Initialize();

	VOXEL_USE_NAMESPACE(Graph);

	if (!Asset->MainEdGraph)
	{
		Asset->MainEdGraph = CreateGraph(Asset);
	}

	for (const UVoxelGraph* Graph : Asset->GetAllGraphs())
	{
		Graph->OnParametersChanged.AddSP(this, &FVoxelGraphToolkit::FixupGraphParameters);
	}

	FVoxelMessages::OnNodeMessageLogged.Add(MakeWeakPtrDelegate(this, [=](const UEdGraph* Graph, const TSharedRef<FTokenizedMessage>& Message)
	{
		check(IsInGameThread());

		if (!Graph ||
			Asset != Graph->GetOuter() ||
			// MessagesListing might be null if the error is triggered from a graph using this graph as macro
			!MessagesListing)
		{
			return;
		}

		const FString MessageString = Message->ToText().ToString();
		if (MessageListingMessages.Contains(MessageString))
		{
			return;
		}

		MessageListingMessages.Add(MessageString);
		MessagesListing->AddMessage(Message);
	}));

	FVoxelMessages::OnClearNodeMessages.Add(MakeWeakPtrDelegate(this, [=](const UEdGraph* Graph)
	{
		check(IsInGameThread());

		if (!Graph ||
			Asset != Graph->GetOuter() ||
			// MessagesListing might be null if the error is triggered from a graph using this graph as macro
			!MessagesListing)
		{
			return;
		}

		MessageListingMessages.Reset();
		MessagesListing->ClearMessages();

		const UEdGraph* ActiveGraph = GetActiveEdGraph();
		if (!ActiveGraph)
		{
			return;
		}

		for (const UEdGraphNode* Node : ActiveGraph->Nodes)
		{
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin->bOrphanedPin)
				{
					continue;
				}

				FVoxelMessageBuilder Builder(EVoxelMessageSeverity::Warning, "Orphaned pin on {0}: {1}");
				FVoxelMessageArgProcessor::ProcessArgs(Builder, Node, Pin->GetDisplayName());

				TSet<const UEdGraph*> Graphs;
				MessagesListing->AddMessage(Builder.BuildMessage(Graphs));
			}
		}
	}));

	{
		ensure(!bDisableOnGraphChanged);
		TGuardValue<bool> OnGraphChangedGuard(bDisableOnGraphChanged, true);

		// Disable SetDirty
		ensure(!GIsEditorLoadingPackage);
		TGuardValue<bool> SetDirtyGuard(GIsEditorLoadingPackage, true);

		const auto FixupEdGraph = [&](UEdGraph* EdGraph)
		{
			if (!ensure(EdGraph))
			{
				return;
			}

			CastChecked<UVoxelEdGraph>(EdGraph)->WeakToolkit = SharedThis(this);

			for (UEdGraphNode* Node : EdGraph->Nodes)
			{
				Node->ReconstructNode();
			}
		};

		for (UEdGraph* EdGraph : GetAllEdGraphs())
		{
			FixupEdGraph(EdGraph);
		}

		for (UVoxelGraph* Graph : Asset->GetAllParameterGraphs())
		{
			if (!Graph->MainEdGraph)
			{
				Graph->MainEdGraph = CreateGraph(Graph);
				continue;
			}

			FixupEdGraph(Graph->MainEdGraph);
		}
	}

	FVoxelSystemUtilities::DelayedCall(MakeWeakPtrLambda(this, [this]
	{
		for (const UEdGraph* EdGraph : GetAllEdGraphs())
		{
			OnGraphChanged(EdGraph);
		}
	}));

	GraphPreview =
		SNew(SVoxelGraphPreview)
		.IsEnabled(!bIsReadOnly)
		.Graph(Asset);

	GraphPreviewStats = GraphPreview->GetPreviewStats();

	GraphMembers =
		SNew(SVoxelGraphMembers)
		.IsEnabled(!bIsReadOnly)
		.Graph(bIsMacroLibrary ? nullptr : Asset)
		.Toolkit(SharedThis(this));

	SearchWidget =
		SNew(SVoxelGraphSearch)
		.Toolkit(SharedThis(this));

	{
		FMessageLogInitializationOptions LogOptions;
		LogOptions.MaxPageCount = 1;
		LogOptions.bAllowClear = false;
		LogOptions.bShowInLogWindow = false;

		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessagesListing = MessageLogModule.CreateLogListing("VoxelGraphErrors", LogOptions);
		MessagesWidget = MessageLogModule.CreateLogListingWidget(MessagesListing.ToSharedRef());
	}

	{
		FDetailsViewArgs Args;
		Args.bAllowSearch = false;
		Args.bShowOptions = false;
		Args.bHideSelectionTip = true;
		Args.bShowPropertyMatrixButton = false;
		Args.NotifyHook = GetNotifyHook();
		Args.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		DetailsView = PropertyModule.CreateDetailView(Args);
		if (!bIsMacroLibrary)
		{
			DetailsView->SetObject(Asset);
		}
		DetailsView->SetEnabled(!bIsReadOnly);

		DetailsTabWidget =
			SAssignNew(ParametersBoxSplitter, SSplitter)
			.Style(FVoxelEditorStyle::Get(), "Members.Splitter")
			.PhysicalSplitterHandleSize(1.f)
			.HitDetectionSplitterHandleSize(5.f)
			.Orientation(Orient_Vertical)
			.ResizeMode(ESplitterResizeMode::Fill)
			+ SSplitter::Slot()
			.Resizable(true)
			[
				DetailsView.ToSharedRef()
			];

		ParameterGraphEditorBox = SNew(SBox);
	}

	{
		FDetailsViewArgs Args;
		Args.bAllowSearch = false;
		Args.bShowOptions = false;
		Args.bHideSelectionTip = true;
		Args.bShowPropertyMatrixButton = false;
		Args.NotifyHook = GetNotifyHook();
		Args.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PreviewDetailsView = PropertyModule.CreateDetailView(Args);
		PreviewDetailsView->SetEnabled(!bIsReadOnly);
		PreviewDetailsView->SetGenericLayoutDetailsDelegate(MakeLambdaDelegate([]() -> TSharedRef<IDetailCustomization>
		{
			return MakeVoxelShared<FVoxelGraphPreviewSettingsCustomization>();
		}));
	}

	for (UEdGraph* Graph : GetAllEdGraphs())
	{
		if (!ensure(Graph->Schema == UVoxelGraphSchema::StaticClass()))
		{
			Graph->Schema = UVoxelGraphSchema::StaticClass();
		}
	}

	GetCommands()->MapAction(
		FVoxelGraphCommands::Get().EnableStats,
		MakeLambdaDelegate([=]
		{
			IVoxelGraphNodeStatInterface::bEnableStats = !IVoxelGraphNodeStatInterface::bEnableStats;

			if (!IVoxelGraphNodeStatInterface::bEnableStats)
			{
				GVoxelOnClearNodeStats.Broadcast();
			}
		}),
		MakeLambdaDelegate([]
		{
			return true;
		}),
		MakeLambdaDelegate([]
		{
			return IVoxelGraphNodeStatInterface::bEnableStats;
		}));

	GetCommands()->MapAction(FGraphEditorCommands::Get().FindReferences,
		FExecuteAction::CreateSP(this, &FVoxelGraphToolkit::FindReferences)
	);

	GetCommands()->MapAction(FVoxelGraphCommands::Get().FindInGraph,
		FExecuteAction::CreateSP(this, &FVoxelGraphToolkit::ToggleSearchTab)
	);

	GetCommands()->MapAction(FVoxelGraphCommands::Get().FindInGraphs,
		FExecuteAction::CreateSP(this, &FVoxelGraphToolkit::ToggleGlobalSearchWindow)
	);

	GetCommands()->MapAction(FVoxelGraphCommands::Get().ReconstructAllNodes,
		FExecuteAction::CreateSP(this, &FVoxelGraphToolkit::ReconstructAllNodes)
	);

	GetCommands()->MapAction(FVoxelGraphCommands::Get().TogglePreview,
		FExecuteAction::CreateSP(this, &FVoxelGraphToolkit::TogglePreview)
	);
}

void FVoxelGraphToolkit::Tick()
{
	VOXEL_FUNCTION_COUNTER();

	Super::Tick();

	FlushNodesToReconstruct();

	if (GraphsToCompile.Num() > 0)
	{
		VOXEL_USE_NAMESPACE(Graph);
		const FCompilationScope CompilationScope;

		const TSet<TWeakObjectPtr<UVoxelGraph>> GraphsToCompileCopy = MoveTemp(GraphsToCompile);
		ensure(GraphsToCompile.Num() == 0);

		for (const TWeakObjectPtr<UVoxelGraph> Graph : GraphsToCompileCopy)
		{
			if (!ensure(Graph.IsValid()))
			{
				continue;
			}

			ensure(!GIsVoxelGraphCompiling);
			GIsVoxelGraphCompiling = true;
			const bool bSuccess = FEditorCompilerUtilities::CompileGraph(*Graph);
			ensure(GIsVoxelGraphCompiling);
			GIsVoxelGraphCompiling = false;

			if (!bSuccess)
			{
				continue;
			}

			Graph->PostGraphCompiled();

			// Compile for errors
			FVoxelGraphMessages Messages(Graph->MainEdGraph);
			FCompilerUtilities::TranslateCompiledGraph(*Graph);
		}

		GraphPreview->QueueUpdate();
	}
}

void FVoxelGraphToolkit::BuildMenu(FMenuBarBuilder& MenuBarBuilder)
{
	Super::BuildMenu(MenuBarBuilder);

	MenuBarBuilder.AddPullDownMenu(
		INVTEXT("Graph"),
		INVTEXT(""),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection("Search", INVTEXT("Search"));
			{
				MenuBuilder.AddMenuEntry(FVoxelGraphCommands::Get().FindInGraph);
				MenuBuilder.AddMenuEntry(FVoxelGraphCommands::Get().FindInGraphs);
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("Tools", INVTEXT("Tools"));
			{
				MenuBuilder.AddMenuEntry(FVoxelGraphCommands::Get().ReconstructAllNodes);
			}
			MenuBuilder.EndSection();
		}));
}

void FVoxelGraphToolkit::BuildToolbar(FToolBarBuilder& ToolbarBuilder)
{
	Super::BuildToolbar(ToolbarBuilder);

	ToolbarBuilder.BeginSection("Voxel");
	ToolbarBuilder.AddToolBarButton(FVoxelGraphCommands::Get().EnableStats);
	ToolbarBuilder.EndSection();
}

TSharedPtr<FTabManager::FLayout> FVoxelGraphToolkit::GetLayout() const
{
	return FTabManager::NewLayout("FVoxelGraphToolkit_Layout_v5")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.15f)
				->SetHideTabWell(true)
				->AddTab(MembersTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.7f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell(true)
					->AddTab("Document", ETabState::ClosedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(MessagesTabId, ETabState::OpenedTab)
					->AddTab(SearchTabId, ETabState::OpenedTab)
					->SetForegroundTab(FTabId(MessagesTabId))
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.15f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.4f)
					->SetHideTabWell(true)
					->AddTab(ViewportTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.1f)
					->SetHideTabWell(true)
					->AddTab(PreviewStatsTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
					->AddTab(PreviewDetailsTabId, ETabState::OpenedTab)
					->SetForegroundTab(FTabId(DetailsTabId))
				)
			)
		);
}

void FVoxelGraphToolkit::RegisterTabs(const FRegisterTab RegisterTab)
{
	RegisterTab(ViewportTabId, INVTEXT("Preview"), "LevelEditor.Tabs.Viewports", GraphPreview);
	RegisterTab(PreviewStatsTabId, INVTEXT("Stats"), "MaterialEditor.ToggleMaterialStats.Tab", GraphPreviewStats);
	RegisterTab(DetailsTabId, INVTEXT("Details"), "LevelEditor.Tabs.Details", DetailsTabWidget);
	RegisterTab(PreviewDetailsTabId, INVTEXT("Preview Settings"), "LevelEditor.Tabs.Details", PreviewDetailsView);
	RegisterTab(MembersTabId, INVTEXT("Members"), "ClassIcon.BlueprintCore", GraphMembers);
	RegisterTab(MessagesTabId, INVTEXT("Messages"), "MessageLog.TabIcon", MessagesWidget);
	RegisterTab(SearchTabId, INVTEXT("Find Results"), "Kismet.Tabs.FindResults", SearchWidget);
}

void FVoxelGraphToolkit::LoadDocuments()
{
	Super::LoadDocuments();

	Asset->LastEditedDocuments.RemoveAll([](const FVoxelEditedDocumentInfo& Document)
	{
		return !Cast<UEdGraph>(Document.EditedObjectPath.ResolveObject());
	});

	if (Asset->LastEditedDocuments.Num() == 0)
	{
		if (bIsMacroLibrary)
		{
			if (Asset->InlineMacros.Num() > 0)
			{
				Asset->LastEditedDocuments.Add(FVoxelEditedDocumentInfo(Asset->InlineMacros[0]->MainEdGraph));
			}
		}
		else
		{
			Asset->LastEditedDocuments.Add(FVoxelEditedDocumentInfo(Asset->MainEdGraph));
		}
	}

	for (const FVoxelEditedDocumentInfo& Document : Asset->LastEditedDocuments)
	{
		const UEdGraph* Graph = Cast<UEdGraph>(Document.EditedObjectPath.ResolveObject());
		if (!ensure(Graph))
		{
			continue;
		}

		const TSharedPtr<SDockTab> TabWithGraph = OpenDocument(Graph, FDocumentTracker::RestorePreviousDocument);
		if (!ensure(TabWithGraph))
		{
			continue;
		}

		const TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(TabWithGraph->GetContent());
		GraphEditor->SetViewLocation(Document.SavedViewOffset, Document.SavedZoomAmount);
	}
}

void FVoxelGraphToolkit::OnGraphChangedImpl(const UEdGraph* EdGraph)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_USE_NAMESPACE(Graph);

	UVoxelGraph* Graph = Asset->FindGraph(EdGraph);
	if (!ensure(Graph))
	{
		return;
	}

	// Fixup reroute nodes
	for (UEdGraphNode* GraphNode : EdGraph->Nodes)
	{
		if (UVoxelGraphKnotNode* Node = Cast<UVoxelGraphKnotNode>(GraphNode))
		{
			Node->PropagatePinType();
		}
	}

	// Fixup parameters
	{
		TSet<FGuid> UnusedParameters;
		for (const FVoxelGraphParameter& Parameter : Graph->Parameters)
		{
			if (Parameter.ParameterType != EVoxelGraphParameterType::Input &&
				Parameter.ParameterType != EVoxelGraphParameterType::Output)
			{
				continue;
			}

			UnusedParameters.Add(Parameter.Guid);
		}

		for (UEdGraphNode* GraphNode : EdGraph->Nodes)
		{
			if (const UVoxelGraphMacroParameterNode* MacroParameterNode = Cast<UVoxelGraphMacroParameterNode>(GraphNode))
			{
				UnusedParameters.Remove(MacroParameterNode->Guid);
			}
			else if (const UVoxelGraphLocalVariableDeclarationNode* LocalVariableNode = Cast<UVoxelGraphLocalVariableDeclarationNode>(GraphNode))
			{
				if (const FVoxelGraphParameter* Parameter = LocalVariableNode->GetParameter())
				{
					if (Parameter->Type.HasPinDefaultValue())
					{
						UEdGraphPin* InputPin = LocalVariableNode->GetInputPin(0);
						Parameter->DefaultValue.ApplyToPinDefaultValue(*InputPin);
					}
				}
			}
		}

		if (UnusedParameters.Num() > 0)
		{
			TGuardValue<bool> Guard(bDisableOnGraphChanged, true);
			const FVoxelTransaction Transaction(Graph);

			for (const FGuid& Guid : UnusedParameters)
			{
				Graph->Parameters.RemoveAll([&](const FVoxelGraphParameter& Parameter)
				{
					return Parameter.Guid == Guid;
				});
			}
		}
	}

	GraphsToCompile.Add(Graph);
}

void FVoxelGraphToolkit::FixupGraphParameters(const UVoxelGraph::EParameterChangeType ChangeType)
{
	VOXEL_FUNCTION_COUNTER();

	for (UVoxelGraph* Graph : Asset->GetAllGraphs())
	{
		FVoxelGraphDelayOnGraphChangedScope DelayScope;

		bool bHasNewParameter = false;
		TArray<UEdGraphNode*> NodesToDelete;

		TSet<FGuid> ParametersToAdd;
		TMap<FGuid, const FVoxelGraphParameter*> Parameters;
		for (const FVoxelGraphParameter& Parameter : Graph->Parameters)
		{
			Parameters.Add(Parameter.Guid, &Parameter);

			if (Parameter.ParameterType == EVoxelGraphParameterType::Input ||
				Parameter.ParameterType == EVoxelGraphParameterType::Output)
			{
				ParametersToAdd.Add(Parameter.Guid);
			}
		}

		for (UEdGraphNode* Node : Graph->MainEdGraph->Nodes)
		{
			if (UVoxelGraphParameterNode* ParameterNode = Cast<UVoxelGraphParameterNode>(Node))
			{
				// Reconstruct to be safe
				ParameterNode->ReconstructNode();
			}
			else if (UVoxelGraphMacroParameterNode* MacroParameterNode = Cast<UVoxelGraphMacroParameterNode>(Node))
			{
				const FVoxelGraphParameter* Parameter = Parameters.FindRef(MacroParameterNode->Guid);
				if (!Parameter)
				{
					NodesToDelete.Add(MacroParameterNode);
					continue;
				}

				ParametersToAdd.Remove(MacroParameterNode->Guid);

				if (ChangeType == UVoxelGraph::EParameterChangeType::DefaultValue)
				{
					MacroParameterNode->PostReconstructNode();
				}
				else
				{
					// Reconstruct to be safe
					MacroParameterNode->ReconstructNode();
				}
			}
			else if (UVoxelGraphLocalVariableNode* LocalVariableNode = Cast<UVoxelGraphLocalVariableNode>(Node))
			{
				if (ChangeType == UVoxelGraph::EParameterChangeType::DefaultValue)
				{
					LocalVariableNode->PostReconstructNode();
				}
				else
				{
					// Reconstruct to be safe
					LocalVariableNode->ReconstructNode();
				}
			}
		}

		for (const FGuid& Guid : ParametersToAdd)
		{
			FVector2D Position;
			Position.X = 400; // TODO: Better search for location...?
			Position.Y = 200; // TODO: Better search for location...?

			FVoxelGraphSchemaAction_NewParameterUsage Action;
			Action.Guid = Guid;
			Action.ParameterType = Graph->FindParameterByGuid(Guid)->ParameterType;
			Action.PerformAction(Graph->MainEdGraph, nullptr, Position);
		}

		bHasNewParameter |= ParametersToAdd.Num() > 0;

		DeleteNodes(NodesToDelete);

		if (bHasNewParameter)
		{
			if (const TSharedPtr<SGraphEditor> GraphEditor = FindGraphEditor(Graph->MainEdGraph))
			{
				GraphEditor->ZoomToFit(true);
			}
		}
	}
}

void FVoxelGraphToolkit::OnGraphEditorFocused(const TSharedRef<SGraphEditor>& GraphEditor)
{
	const UEdGraph* EdGraph = GraphEditor->GetCurrentGraph();
	if (!ensure(EdGraph))
	{
		return;
	}

	UVoxelGraph* Graph = Asset->FindGraph(EdGraph);
	// Fails to find graph, after graph removal with undo
	if (!Graph)
	{
		return;
	}

	GraphPreview->WeakGraph = Graph;
	GraphPreview->QueueUpdate();

	GraphMembers->UpdateActiveGraph(Graph);

	if (UEdGraphNode* SelectedNode = GraphEditor->GetSingleSelectedNode())
	{
		UpdateDetailsView(SelectedNode);
	}
	else
	{
		UpdateDetailsView(Graph);
	}

	GraphMembers->RequestRefresh();
}

void FVoxelGraphToolkit::OnGraphEditorClosed(TSharedRef<SDockTab> TargetGraph)
{
	Super::OnGraphEditorClosed(TargetGraph);

	GraphMembers->UpdateActiveGraph(nullptr);
	GraphMembers->RequestRefresh();
}

FString FVoxelGraphToolkit::GetGraphName(UEdGraph* EdGraph) const
{
	if (!ensure(EdGraph))
	{
		return {};
	}

	const UVoxelGraph* Graph = Asset->FindGraph(EdGraph);
	// Fails to find graph, after graph removal with undo
	if (!Graph)
	{
		return {};
	}

	if (Graph == Asset)
	{
		FString Name = Asset->GetClass()->GetDisplayNameText().ToString();
		ensure(Name.RemoveFromStart("Voxel "));
		return Name;
	}

	const UVoxelGraph* MacroGraph = Cast<UVoxelGraph>(Graph);
	if (!ensure(MacroGraph))
	{
		return {};
	}

	return MacroGraph->GetGraphName();
}

const FSlateBrush* FVoxelGraphToolkit::GetGraphIcon(UEdGraph* EdGraph) const
{
	if (!ensure(EdGraph))
	{
		return nullptr;
	}

	if (Asset->FindGraph(EdGraph) == Asset)
	{
		return FAppStyle::GetBrush(TEXT("GraphEditor.EventGraph_16x"));
	}

	return FAppStyle::GetBrush(TEXT("GraphEditor.Macro_16x"));
}

void FVoxelGraphToolkit::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	VOXEL_USE_NAMESPACE(Graph);

	if (NewSelection.Num() == 0)
	{
		UpdateDetailsView(GetActiveEdGraph());
		SelectMember(nullptr, false, false);
		return;
	}

	if (NewSelection.Num() != 1)
	{
		UpdateDetailsView(nullptr);
		SelectMember(nullptr, false, false);
		return;
	}

	for (UObject* Node : NewSelection)
	{
		UpdateDetailsView(Node);
		SelectMember(Node, false, false);
		return;
	}
}

void FVoxelGraphToolkit::SaveTabState(const FVoxelEditedDocumentInfo& EditedDocumentInfo)
{
	Asset->LastEditedDocuments.Add(EditedDocumentInfo);
}

void FVoxelGraphToolkit::ClearSavedDocuments()
{
	Asset->LastEditedDocuments.Empty();
}

TArray<UEdGraph*> FVoxelGraphToolkit::GetAllEdGraphs() const
{
	TArray<UEdGraph*> EdGraphs;
	for (const UVoxelGraph* Graph : Asset->GetAllGraphs())
	{
		if (ensure(Graph->MainEdGraph))
		{
			EdGraphs.Add(Graph->MainEdGraph);
		}
	}
	return EdGraphs;
}

void FVoxelGraphToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);

	GraphPreview->AddReferencedObjects(Collector);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGraphToolkit::FlushNodesToReconstruct()
{
	VOXEL_FUNCTION_COUNTER();

	const TSet<TWeakObjectPtr<UEdGraphNode>> NodesToReconstructCopy = MoveTemp(NodesToReconstruct);
	ensure(NodesToReconstruct.Num() == 0);

	for (const TWeakObjectPtr<UEdGraphNode>& WeakNode : NodesToReconstructCopy)
	{
		UEdGraphNode* Node = WeakNode.Get();
		if (!ensure(Node))
		{
			continue;
		}

		Node->ReconstructNode();
	}
}

FVector2D FVoxelGraphToolkit::FindLocationInGraph() const
{
	const TSharedPtr<SGraphEditor> GraphEditor = GetActiveGraphEditor();
	UEdGraph* EdGraph = GetActiveEdGraph();

	if (!GraphEditor)
	{
		if (!EdGraph)
		{
			return FVector2D::ZeroVector;
		}

		return EdGraph->GetGoodPlaceForNewNode();
	}

	const FGeometry& CachedGeometry = GraphEditor->GetCachedGeometry();
	const FVector2D CenterLocation = GraphEditor->GetGraphPanel()->PanelCoordToGraphCoord(CachedGeometry.GetLocalSize() / 2.f);

	FVector2D Location = CenterLocation;

	const FVector2D StaticSize = FVector2D(100.f, 50.f);
	FBox2D CurrentBox(Location, Location + StaticSize);

	const FBox2D ViewportBox(
		GraphEditor->GetGraphPanel()->PanelCoordToGraphCoord(FVector2D::ZeroVector),
		GraphEditor->GetGraphPanel()->PanelCoordToGraphCoord(CachedGeometry.GetLocalSize()));

	if (!EdGraph)
	{
		return Location;
	}

	int32 Iterations = 0;
	bool bFoundLocation = false;
	while (!bFoundLocation)
	{
		bFoundLocation = true;
		for (int32 Index = 0; Index < EdGraph->Nodes.Num(); Index++)
		{
			const UEdGraphNode* CurrentNode = EdGraph->Nodes[Index];
			const FVector2D NodePosition(CurrentNode->NodePosX, CurrentNode->NodePosY);
			FBox2D NodeBox(NodePosition, NodePosition);

			if (const TSharedPtr<SGraphNode> Widget = GraphEditor->GetGraphPanel()->GetNodeWidgetFromGuid(CurrentNode->NodeGuid))
			{
				if (Widget->GetCachedGeometry().GetLocalSize() == FVector2D::ZeroVector)
				{
					continue;
				}

				NodeBox.Max += Widget->GetCachedGeometry().GetAbsoluteSize() / GraphEditor->GetGraphPanel()->GetZoomAmount();
			}
			else
			{
				NodeBox.Max += StaticSize;
			}

			if (CurrentBox.Intersect(NodeBox))
			{
				Location.Y = NodeBox.Max.Y + 30.f;

				CurrentBox.Min = Location;
				CurrentBox.Max = Location + StaticSize;

				bFoundLocation = false;
				break;
			}
		}

		if (!CurrentBox.Intersect(ViewportBox))
		{
			Iterations++;
			if (Iterations == 1)
			{
				Location = CenterLocation + FVector2D(200.f, 0.f);
			}
			else if (Iterations == 2)
			{
				Location = CenterLocation - FVector2D(200.f, 0.f);
			}
			else
			{
				Location = CenterLocation;
				break;
			}

			CurrentBox.Min = Location;
			CurrentBox.Max = Location + StaticSize;

			bFoundLocation = false;
		}
	}

	return Location;
}

void FVoxelGraphToolkit::FindReferences() const
{
	const TSet<UEdGraphNode*> SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() != 1)
	{
		return;
	}
	const UEdGraphNode* Node = *SelectedNodes.CreateConstIterator();

	if (const UVoxelGraphParameterNodeBase* ParameterNode = Cast<UVoxelGraphParameterNodeBase>(Node))
	{
		SearchWidget->FocusForUse(ParameterNode->Guid.ToString());
	}
	else
	{
		SearchWidget->FocusForUse(Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"), TEXT(" ")));
	}
}

void FVoxelGraphToolkit::ToggleSearchTab() const
{
	SearchWidget->FocusForUse({});
}

void FVoxelGraphToolkit::ToggleGlobalSearchWindow() const
{
	if (const TSharedPtr<SVoxelGraphSearch> GlobalSearch = FVoxelGraphSearchManager::Get().OpenGlobalSearch())
	{
		GlobalSearch->FocusForUse({});
	}
}

void FVoxelGraphToolkit::ReconstructAllNodes() const
{
	for (UEdGraph* EdGraph : GetAllEdGraphs())
	{
		if (!ensure(EdGraph))
		{
			continue;
		}

		for (UEdGraphNode* Node : EdGraph->Nodes)
		{
			if (!ensure(Node))
			{
				continue;
			}

			Node->ReconstructNode();
		}
	}
}

void FVoxelGraphToolkit::TogglePreview() const
{
	TVoxelArray<UVoxelGraphNodeBase*> Nodes;
	for (UEdGraphNode* Node : GetSelectedNodes())
	{
		if (UVoxelGraphNodeBase* VoxelNode = Cast<UVoxelGraphNodeBase>(Node))
		{
			Nodes.Add(VoxelNode);
		}
	}

	if (Nodes.Num() != 1)
	{
		return;
	}

	UVoxelGraphNodeBase* Node = Nodes[0];

	if (Node->bEnablePreview)
	{
		const FVoxelTransaction Transaction(Node, "Stop previewing");
		Node->bEnablePreview = false;
		return;
	}

	const FVoxelTransaction Transaction(Node, "Start previewing");

	for (const TObjectPtr<UEdGraphNode>& OtherNode : Node->GetGraph()->Nodes)
	{
		if (UVoxelGraphNodeBase* OtherVoxelNode = Cast<UVoxelGraphNodeBase>(OtherNode.Get()))
		{
			if (OtherVoxelNode->bEnablePreview)
			{
				const FVoxelTransaction OtherTransaction(OtherVoxelNode, "Stop previewing");
				OtherVoxelNode->bEnablePreview = false;
			}
		}
	}

	Node->bEnablePreview = true;
}

void FVoxelGraphToolkit::UpdateDetailsView(UObject* Object)
{
	VOXEL_USE_NAMESPACE(Graph);

	if (const UVoxelGraphParameterNodeBase* ParameterNode = Cast<UVoxelGraphParameterNodeBase>(Object))
	{
		DetailsView->SetGenericLayoutDetailsDelegate(MakeLambdaDelegate([this, ParameterNode]() -> TSharedRef<IDetailCustomization>
		{
			return MakeVoxelShared<FVoxelGraphParameterSelectionCustomization>(SharedThis(this), ParameterNode->Guid);
		}));
	}
	else if (UVoxelGraphStructNode* StructNode = Cast<UVoxelGraphStructNode>(Object))
	{
		DetailsView->SetGenericLayoutDetailsDelegate(MakeLambdaDelegate([this]() -> TSharedRef<IDetailCustomization>
		{
			return MakeVoxelShared<FVoxelGraphNodeCustomization>();
		}));
	}
	else
	{
		DetailsView->SetGenericLayoutDetailsDelegate(nullptr);
	}

	UVoxelGraph* ParameterGraph = nullptr;
	if (const UEdGraph* EdGraph = Cast<UEdGraph>(Object))
	{
		if (UVoxelGraph* Graph = Asset->FindGraph(EdGraph))
		{
			DetailsView->SetObject(Graph);
		}
		else
		{
			if (!Graph)
			{
				if (TSharedPtr<SGraphEditor> GraphEditor = FindGraphEditor(EdGraph))
				{
					CloseInvalidGraphs();
				}
			}
			DetailsView->SetObject(nullptr);
		}
	}
	else if (const UVoxelGraphParameterNodeBase* ParameterNode = Cast<UVoxelGraphParameterNodeBase>(Object))
	{
		UVoxelGraph* Graph = Asset->FindGraph(ParameterNode->GetGraph());
		if (ensure(Graph))
		{
			ParameterGraph = Graph->ParameterGraphs.FindRef(ParameterNode->Guid);
			DetailsView->SetObject(Graph);
		}
		else
		{
			DetailsView->SetObject(nullptr);
		}
	}
	else
	{
		DetailsView->SetObject(Object);
	}

	PreviewDetailsView->SetObject(Cast<UVoxelGraphNode>(Object));

	DetailsView->ForceRefresh();
	PreviewDetailsView->ForceRefresh();

	UpdateParameterGraph(ParameterGraph);
}

void FVoxelGraphToolkit::SelectMember(UObject* Object, const bool bRequestRename, const bool bRefreshMembers) const
{
	VOXEL_USE_NAMESPACE(Graph);

	if (UEdGraph* EdGraph = Cast<UEdGraph>(Object))
	{
		const UVoxelGraph* Graph = Asset->FindGraph(EdGraph);

		EMembersNodeSection Section = EMembersNodeSection::None;
		if (bIsMacroLibrary)
		{
			Section = EMembersNodeSection::MacroLibraries;
		}
		else if (Graph != Asset)
		{
			Section = EMembersNodeSection::InlineMacros;
		}

		GraphMembers->SelectMember(FName(GetGraphName(EdGraph)), GetSectionId(Section), bRequestRename, bRefreshMembers);
		return;
	}

	if (const UVoxelGraph* Graph = Cast<UVoxelGraph>(Object))
	{
		EMembersNodeSection Section = EMembersNodeSection::None;
		if (bIsMacroLibrary)
		{
			Section = EMembersNodeSection::MacroLibraries;
		}
		else if (Graph != Asset)
		{
			Section = EMembersNodeSection::InlineMacros;
		}

		GraphMembers->SelectMember(FName(GetGraphName(Graph->MainEdGraph)), GetSectionId(Section), bRequestRename, bRefreshMembers);
		return;
	}

	if (const UVoxelGraphParameterNodeBase* ParameterNode = Cast<UVoxelGraphParameterNodeBase>(Object))
	{
		GraphMembers->SelectMember(ParameterNode->CachedParameter.Name, GetSectionId(ParameterNode->CachedParameter.ParameterType), bRequestRename, bRefreshMembers);
		return;
	}

	GraphMembers->SelectMember("", GetSectionId(EMembersNodeSection::None), false, bRefreshMembers);
}

void FVoxelGraphToolkit::SelectParameter(UVoxelGraph* Graph, FGuid Guid, const bool bRequestRename, const bool bRefreshMembers)
{
	VOXEL_USE_NAMESPACE(Graph);

	const FVoxelGraphParameter* Parameter = Graph->FindParameterByGuid(Guid);
	if (!ensure(Parameter))
	{
		UpdateDetailsView(GetActiveEdGraph());
		SelectMember(nullptr, false, true);
		return;
	}

	GraphMembers->SelectMember(Parameter->Name, GetSectionId(Parameter->ParameterType), bRequestRename, bRefreshMembers);

	DetailsView->SetGenericLayoutDetailsDelegate(MakeLambdaDelegate([this, Guid]() -> TSharedRef<IDetailCustomization>
	{
		return MakeVoxelShared<FVoxelGraphParameterSelectionCustomization>(SharedThis(this), Guid);
	}));
	DetailsView->SetObject(Graph);
	DetailsView->ForceRefresh();

	UpdateParameterGraph(Graph->ParameterGraphs.FindRef(Guid));
}

void FVoxelGraphToolkit::UpdateParameterGraph(UVoxelGraph* Graph)
{
	if (!Graph ||
		!Graph->bParameterGraph)
	{
		if (ParametersBoxSplitter->GetChildren()->Num() > 1)
		{
			ParametersBoxSplitter->RemoveAt(1);
		}

		ParameterGraphEditorBox->SetContent(SNullWidget::NullWidget);
		return;
	}

	if (ParametersBoxSplitter->GetChildren()->Num() == 1)
	{
		ParametersBoxSplitter->AddSlot()
		.Resizable(true)
		[
			ParameterGraphEditorBox.ToSharedRef()
		];
	}

	if (!Graph->MainEdGraph)
	{
		Graph->MainEdGraph = CreateGraph(Graph);
	}

	ParameterGraphEditorBox->SetContent(CreateGraphEditor(Graph->MainEdGraph, false));
}

void FVoxelGraphToolkit::RefreshPreviewSettings() const
{
	if (PreviewDetailsView)
	{
		PreviewDetailsView->ForceRefresh();
	}
}