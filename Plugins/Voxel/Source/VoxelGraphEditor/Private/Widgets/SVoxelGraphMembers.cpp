// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SVoxelGraphMembers.h"

#include "VoxelEdGraph.h"
#include "VoxelGraphSchema.h"
#include "VoxelGraphToolkit.h"
#include "SchemaActions/VoxelGraphMembersGraphSchemaAction.h"
#include "SchemaActions/VoxelGraphMembersMacroSchemaAction.h"
#include "SchemaActions/VoxelGraphMembersVariableSchemaAction.h"
#include "SchemaActions/VoxelGraphMembersMacroLibrarySchemaAction.h"

BEGIN_VOXEL_NAMESPACE(Graph)

void SMembers::Construct(const FArguments& Args)
{
	SVoxelMembers::Construct(
		SVoxelMembers::FArguments()
		.Object(Args._Graph)
		.Toolkit(Args._Toolkit));

	const TSharedPtr<FVoxelGraphToolkit> Toolkit = GetToolkit<FVoxelGraphToolkit>();
	if (!ensure(Toolkit))
	{
		return;
	}

	const UVoxelGraph* MainGraph = Toolkit->Asset;
	if (!ensure(MainGraph))
	{
		return;
	}

	MainGraph->OnParametersChanged.AddSP(this, &SMembers::OnParametersChanged);

	UpdateActiveGraph(GetObject<UVoxelGraph>());
}

void SMembers::UpdateActiveGraph(const TWeakObjectPtr<UVoxelGraph>& NewActiveGraph)
{
	const TSharedPtr<FVoxelGraphToolkit> Toolkit = GetToolkit<FVoxelGraphToolkit>();
	if (!ensure(Toolkit))
	{
		return;
	}

	const UVoxelGraph* MainGraph = Toolkit->Asset;
	if (!ensure(MainGraph))
	{
		return;
	}

	if (const UVoxelGraph* ActiveGraph = GetObject<UVoxelGraph>())
	{
		if (OnMembersChangedHandle.IsValid())
		{
			ActiveGraph->OnParametersChanged.Remove(OnMembersChangedHandle);
		}
	}

	SetObject(NewActiveGraph);

	const UVoxelGraph* ActiveGraph = GetObject<UVoxelGraph>();
	if (!ActiveGraph ||
		ActiveGraph == MainGraph)
	{
		return;
	}

	OnMembersChangedHandle = ActiveGraph->OnParametersChanged.AddSP(this, &SMembers::OnParametersChanged);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SMembers::CollectStaticSections(TArray<int32>& StaticSectionIds)
{
	const TSharedPtr<FVoxelGraphToolkit> Toolkit = GetToolkit<FVoxelGraphToolkit>();
	if (Toolkit->bIsMacroLibrary)
	{
		StaticSectionIds.Add(GetSectionId(EMembersNodeSection::MacroLibraries));
	}
	else
	{
		StaticSectionIds.Add(GetSectionId(EMembersNodeSection::Graph));
		StaticSectionIds.Add(GetSectionId(EMembersNodeSection::InlineMacros));
	}

	const UVoxelGraph* Graph = GetObject<UVoxelGraph>();
	if (!Graph)
	{
		return;
	}

	StaticSectionIds.Add(GetSectionId(EMembersNodeSection::MacroInputs));
	StaticSectionIds.Add(GetSectionId(EMembersNodeSection::MacroOutputs));

	if (!Toolkit->bIsMacroLibrary)
	{
		StaticSectionIds.Add(GetSectionId(EMembersNodeSection::Parameters));
	}

	StaticSectionIds.Add(GetSectionId(EMembersNodeSection::LocalVariables));
}

FText SMembers::OnGetSectionTitle(const int32 SectionId)
{
	static const TArray<FText> NodeSectionNames
	{
		INVTEXT(""),
		INVTEXT("Graph"),
		INVTEXT("Macros"),
		INVTEXT("Macros"),
		INVTEXT("Parameters"),
		INVTEXT("Inputs"),
		INVTEXT("Outputs"),
		INVTEXT("Local Variables"),
	};

	if (!ensure(NodeSectionNames.IsValidIndex(SectionId)))
	{
		return {};
	}

	return NodeSectionNames[SectionId];
}

TSharedRef<SWidget> SMembers::OnGetMenuSectionWidget(TSharedRef<SWidget> RowWidget, int32 SectionId)
{
	switch (GetSection(SectionId))
	{
	default: check(false);
	case EMembersNodeSection::Graph: return SNullWidget::NullWidget;
	case EMembersNodeSection::InlineMacros: return CreateAddButton(SectionId, INVTEXT("Macro"), "AddNewMacro");
	case EMembersNodeSection::MacroLibraries: return CreateAddButton(SectionId, INVTEXT("Macro"), "AddNewMacro");
	case EMembersNodeSection::Parameters: return CreateAddButton(SectionId, INVTEXT("Parameter"), "AddNewParameter");
	case EMembersNodeSection::MacroInputs: return CreateAddButton(SectionId, INVTEXT("Input"), "AddNewInput");
	case EMembersNodeSection::MacroOutputs: return CreateAddButton(SectionId, INVTEXT("Output"), "AddNewOutput");
	case EMembersNodeSection::LocalVariables: return CreateAddButton(SectionId, INVTEXT("Local Variable"), "AddNewLocalVariable");
	}
}

void SMembers::CollectSortedActions(FVoxelMembersActionsSortHelper& OutActionsList)
{
	const TSharedPtr<FVoxelGraphToolkit> Toolkit = GetToolkit<FVoxelGraphToolkit>();
	if (!ensure(Toolkit))
	{
		return;
	}

	UVoxelGraph& MainGraph = *Toolkit->Asset;

	OutActionsList.AddCategoriesSortList(GetSectionId(EMembersNodeSection::None), {});

	if (Toolkit->bIsMacroLibrary)
	{
		OutActionsList.AddCategoriesSortList(GetSectionId(EMembersNodeSection::MacroLibraries), MainGraph.InlineMacroCategories.Categories);
	}
	else
	{
		OutActionsList.AddCategoriesSortList(GetSectionId(EMembersNodeSection::Graph), {});
		OutActionsList.AddCategoriesSortList(GetSectionId(EMembersNodeSection::InlineMacros), MainGraph.InlineMacroCategories.Categories);
	}

	INLINE_LAMBDA
	{
		if (Toolkit->bIsMacroLibrary)
		{
			for (UVoxelGraph* Macro : MainGraph.InlineMacros)
			{
				TSharedRef<FVoxelGraphMembersMacroLibrarySchemaAction> NewMacroAction = MakeVoxelShared<FVoxelGraphMembersMacroLibrarySchemaAction>(
					FText::FromString(Macro->Category),
					FText::FromString(Macro->GetGraphName()),
					FText::FromString(Macro->Description),
					0,
					INVTEXT("macro"),
					GetSectionId(EMembersNodeSection::MacroLibraries));

				NewMacroAction->WeakToolkit = Toolkit;
				NewMacroAction->WeakMembersWidget = SharedThis(this);
				NewMacroAction->WeakMainGraph = &MainGraph;
				NewMacroAction->WeakMacroGraph = Macro;

				OutActionsList.AddAction(NewMacroAction, Macro->Category);
			}
			return;
		}

		{
			const TSharedRef<FVoxelGraphMembersGraphSchemaAction> NewGraphAction = MakeVoxelShared<FVoxelGraphMembersGraphSchemaAction>(
				INVTEXT(""),
				FText::FromString(Toolkit->GetGraphName(MainGraph.MainEdGraph)),
				INVTEXT(""),
				2,
				INVTEXT(""),
				GetSectionId(EMembersNodeSection::Graph));

			NewGraphAction->WeakToolkit = Toolkit;
			NewGraphAction->WeakMembersWidget = SharedThis(this);
			NewGraphAction->WeakGraph = &MainGraph;

			OutActionsList.AddAction(NewGraphAction, "");
		}

		for (UVoxelGraph* InlineMacro : MainGraph.InlineMacros)
		{
			TSharedRef<FVoxelGraphMembersMacroSchemaAction> NewMacroAction = MakeVoxelShared<FVoxelGraphMembersMacroSchemaAction>(
				FText::FromString(InlineMacro->Category),
				FText::FromString(InlineMacro->GetGraphName()),
				FText::FromString(InlineMacro->Description),
				2,
				INVTEXT("macro"),
				GetSectionId(EMembersNodeSection::InlineMacros));

			NewMacroAction->WeakToolkit = Toolkit;
			NewMacroAction->WeakMembersWidget = SharedThis(this);
			NewMacroAction->WeakMainGraph = &MainGraph;
			NewMacroAction->WeakMacroGraph = InlineMacro;

			OutActionsList.AddAction(NewMacroAction, InlineMacro->Category);
		}
	};

	UVoxelGraph* ActiveGraph = GetObject<UVoxelGraph>();
	if (!ActiveGraph)
	{
		return;
	}

	OutActionsList.AddCategoriesSortList(GetSectionId(EMembersNodeSection::MacroInputs), ActiveGraph->GetCategories(EVoxelGraphParameterType::Input));
	OutActionsList.AddCategoriesSortList(GetSectionId(EMembersNodeSection::MacroOutputs), ActiveGraph->GetCategories(EVoxelGraphParameterType::Output));

	if (!Toolkit->bIsMacroLibrary)
	{
		OutActionsList.AddCategoriesSortList(GetSectionId(EMembersNodeSection::Parameters), ActiveGraph->GetCategories(EVoxelGraphParameterType::Parameter));
	}

	OutActionsList.AddCategoriesSortList(GetSectionId(EMembersNodeSection::LocalVariables), ActiveGraph->GetCategories(EVoxelGraphParameterType::LocalVariable));

	for (const FVoxelGraphParameter& Parameter : ActiveGraph->Parameters)
	{
		TSharedRef<FVoxelGraphMembersVariableSchemaAction> NewParameterAction = MakeVoxelShared<FVoxelGraphMembersVariableSchemaAction>(
			FText::FromString(Parameter.Category),
			FText::FromName(Parameter.Name),
			FText::FromString(Parameter.Description),
			1,
			FText::FromString(Parameter.Type.ToString()),
			GetSectionId(Parameter.ParameterType));

		NewParameterAction->WeakToolkit = Toolkit;
		NewParameterAction->WeakMembersWidget = SharedThis(this);
		NewParameterAction->WeakOwnerGraph = ActiveGraph;
		NewParameterAction->ParameterGuid = Parameter.Guid;

		OutActionsList.AddAction(NewParameterAction, Parameter.Category);
	}
}

void SMembers::SelectBaseObject()
{
	if (const TSharedPtr<FVoxelGraphToolkit> Toolkit = GetToolkit<FVoxelGraphToolkit>())
	{
		Toolkit->UpdateDetailsView(Toolkit->GetActiveEdGraph());
		Toolkit->SelectMember(nullptr, false, false);
	}
}

void SMembers::GetContextMenuAddOptions(FMenuBuilder& MenuBuilder)
{
	const TSharedPtr<FVoxelGraphToolkit> Toolkit = GetToolkit<FVoxelGraphToolkit>();
	if (!ensure(Toolkit))
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		INVTEXT("Add new Macro"),
		FText(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintEditor.AddNewMacroDeclaration"),
		FUIAction{
			FExecuteAction::CreateSP(this, &SMembers::OnAddNewMember, GetSectionId(Toolkit->bIsMacroLibrary ? EMembersNodeSection::MacroLibraries : EMembersNodeSection::InlineMacros))
		});

	if (!GetObject<UVoxelGraph>())
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		INVTEXT("Add new Input"),
		FText(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintEditor.AddNewVariable"),
		FUIAction{
			FExecuteAction::CreateSP(this, &SMembers::OnAddNewMember, GetSectionId(EMembersNodeSection::MacroInputs))
		});
	MenuBuilder.AddMenuEntry(
		INVTEXT("Add new Output"),
		FText(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintEditor.AddNewVariable"),
		FUIAction{
			FExecuteAction::CreateSP(this, &SMembers::OnAddNewMember, GetSectionId(EMembersNodeSection::MacroOutputs))
		});

	MenuBuilder.AddMenuEntry(
		INVTEXT("Add new Parameter"),
		FText(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintEditor.AddNewVariable"),
		FUIAction{
			FExecuteAction::CreateSP(this, &SMembers::OnAddNewMember, GetSectionId(EMembersNodeSection::Parameters))
		});

	MenuBuilder.AddMenuEntry(
		INVTEXT("Add new Local Variable"),
		FText(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintEditor.AddNewLocalVariable"),
		FUIAction{
			FExecuteAction::CreateSP(this, &SMembers::OnAddNewMember, GetSectionId(EMembersNodeSection::LocalVariables))
		});
}

void SMembers::OnPasteItem(const FString& ImportText, int32 SectionId)
{
	UVoxelGraph* Graph = GetObject<UVoxelGraph>();
	const TSharedPtr<FVoxelGraphToolkit> Toolkit = GetToolkit<FVoxelGraphToolkit>();
	if (!Graph ||
		!ensure(Toolkit))
	{
		return;
	}

	FStringOutputDevice Errors;
	const EMembersNodeSection Section = GetSection(SectionId);

	if (Section == EMembersNodeSection::InlineMacros ||
		Section == EMembersNodeSection::MacroLibraries)
	{
		const UVoxelGraph* MacroGraph = FindObject<UVoxelGraph>(nullptr, *ImportText);
		if (!MacroGraph)
		{
			return;
		}

		FName NewMacroName = *MacroGraph->GetGraphName();

		UVoxelGraph* MainGraph = Toolkit->Asset;

		TSet<FName> UsedNames;
		for (const UVoxelGraph* InlineMacro : MainGraph->InlineMacros)
		{
			UsedNames.Add(*InlineMacro->GetGraphName());
		}

		while (UsedNames.Contains(NewMacroName))
		{
			NewMacroName.SetNumber(NewMacroName.GetNumber() + 1);
		}

		UVoxelGraph* NewGraph;

		{
			const FVoxelTransaction Transaction(MainGraph, "Paste macro");

			NewGraph = DuplicateObject<UVoxelGraph>(MacroGraph, MainGraph, NAME_Name);
			NewGraph->MainEdGraph = DuplicateObject<UVoxelEdGraph>(Cast<UVoxelEdGraph>(MacroGraph->MainEdGraph), NewGraph, NAME_None);
			NewGraph->SetGraphName(NewMacroName.ToString());
			NewGraph->OnParametersChanged.AddSP(Toolkit.Get(), &FVoxelGraphToolkit::FixupGraphParameters);
			Cast<UVoxelEdGraph>(NewGraph->MainEdGraph)->WeakToolkit = Toolkit;

			MainGraph->InlineMacros.Add(NewGraph);
		}

		Toolkit->UpdateDetailsView(NewGraph);
		Toolkit->OpenGraphAndBringToFront(NewGraph->MainEdGraph, false);
		Toolkit->SelectMember(NewGraph, true, true);
	}
	else if (
		Section == EMembersNodeSection::Parameters ||
		Section == EMembersNodeSection::MacroInputs ||
		Section == EMembersNodeSection::MacroOutputs ||
		Section == EMembersNodeSection::LocalVariables)
	{
		FVoxelGraphParameter NewParameter;
		FVoxelGraphParameter::StaticStruct()->ImportText(
			*ImportText,
			&NewParameter,
			nullptr,
			0,
			&Errors,
			FVoxelGraphParameter::StaticStruct()->GetName());

		if (!Errors.IsEmpty())
		{
			return;
		}

		NewParameter.Guid = FGuid::NewGuid();
		NewParameter.Category = GetPasteCategory();

		{
			const FVoxelTransaction Transaction(Graph, "Paste " + OnGetSectionTitle(SectionId).ToString());
			Graph->Parameters.Add(NewParameter);
		}

		Toolkit->SelectParameter(Graph, NewParameter.Guid, true, true);
	}
}

bool SMembers::CanPasteItem(const FString& ImportText, int32 SectionId)
{
	FStringOutputDevice Errors;
	const EMembersNodeSection Section = GetSection(SectionId);

	if (Section == EMembersNodeSection::InlineMacros ||
		Section == EMembersNodeSection::MacroLibraries)
	{
		return FindObject<UVoxelGraph>(nullptr, *ImportText) != nullptr;
	}
	else if (
		Section == EMembersNodeSection::Parameters ||
		Section == EMembersNodeSection::MacroInputs ||
		Section == EMembersNodeSection::MacroOutputs ||
		Section == EMembersNodeSection::LocalVariables)
	{
		FVoxelGraphParameter Parameter;
		FVoxelGraphParameter::StaticStruct()->ImportText(
			*ImportText,
			&Parameter,
			nullptr,
			0,
			&Errors,
			FVoxelGraphParameter::StaticStruct()->GetName());
	}
	else
	{
		return false;
	}

	return Errors.IsEmpty();
}

void SMembers::OnAddNewMember(int32 SectionId)
{
	const TSharedPtr<FVoxelGraphToolkit> Toolkit = GetToolkit<FVoxelGraphToolkit>();
	if (!ensure(Toolkit))
	{
		return;
	}

	const EMembersNodeSection Section = GetSection(SectionId);
	if (!ensure(Section != EMembersNodeSection::None) ||
		!ensure(Section != EMembersNodeSection::Graph))
	{
		return;
	}

	if (Section == EMembersNodeSection::InlineMacros ||
		Section == EMembersNodeSection::MacroLibraries)
	{
		FVoxelGraphSchemaAction_NewInlineMacro Action;
		Action.TargetCategory = GetPasteCategory();
		Action.PerformAction(Toolkit->Asset->MainEdGraph, nullptr, Toolkit->FindLocationInGraph());
		return;
	}

	UEdGraph* EdGraph = Toolkit->GetActiveEdGraph();
	if (!ensure(EdGraph))
	{
		return;
	}

	FVoxelGraphSchemaAction_NewParameter Action;
	Action.TargetCategory = GetPasteCategory();
	Action.ParameterType = GetParameterType(Section);
	Action.PerformAction(EdGraph, nullptr, Toolkit->FindLocationInGraph());
}

const TArray<FString>& SMembers::GetCopyPrefixes() const
{
	static const TArray<FString> CopyPrefixes
	{
		"INVALID:",
		"INVALID:",
		"GraphInlineMacro:",
		"GraphMacroLibrary:",
		"GraphParameter:",
		"GraphMacroInput:",
		"GraphMacroOutput:",
		"GraphLocalVariable:",
	};

	return CopyPrefixes;
}

TArray<FString>& SMembers::GetEditableCategories(const int32 SectionId)
{
	const EMembersNodeSection Section = GetSection(SectionId);
	if (Section == EMembersNodeSection::InlineMacros ||
		Section == EMembersNodeSection::MacroLibraries)
	{
		return GetToolkit<FVoxelGraphToolkit>()->Asset->InlineMacroCategories.Categories;
	}

	UVoxelGraph* Graph = GetObject<UVoxelGraph>();
	if (!ensure(Graph))
	{
		static TArray<FString> CategoriesList;
		return CategoriesList;
	}

	return Graph->GetCategories(GetParameterType(Section));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SMembers::OnParametersChanged(const UVoxelGraph::EParameterChangeType ChangeType)
{
	switch (ChangeType)
	{
	default: check(false);
	case UVoxelGraph::EParameterChangeType::Unknown: RequestRefresh(); break;
	case UVoxelGraph::EParameterChangeType::DefaultValue: break;
	}
}

END_VOXEL_NAMESPACE(Graph)