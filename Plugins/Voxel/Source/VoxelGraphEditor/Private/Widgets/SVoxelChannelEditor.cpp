// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SVoxelChannelEditor.h"

#include "VoxelChannel.h"
#include "VoxelSettings.h"
#include "VoxelGraphVisuals.h"
#include "Customizations/VoxelChannelEditorDefinitionCustomization.h"

#include "ContentBrowserModule.h"
#include "IStructureDetailsView.h"

void SVoxelChannelEditor::Construct(const FArguments& InArgs)
{
	MaxHeight = InArgs._MaxHeight;
	SelectedChannel = InArgs._SelectedChannel;
	OnChannelSelectedDelegate = InArgs._OnChannelSelected;

	MapChannels();
	CreateDetailsView();

	ChildSlot
	[
		SNew(SBorder)
		.Padding(2.f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]
				{
					return bAddNewVisible ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState)
				{
					bAddNewVisible = NewState == ECheckBoxState::Checked;
				})
				.CheckedImage(FAppStyle::GetBrush("TreeArrow_Expanded"))
				.CheckedHoveredImage(FAppStyle::GetBrush("TreeArrow_Expanded_Hovered"))
				.CheckedPressedImage(FAppStyle::GetBrush("TreeArrow_Expanded"))
				.UncheckedImage(FAppStyle::GetBrush("TreeArrow_Collapsed"))
				.UncheckedHoveredImage(FAppStyle::GetBrush("TreeArrow_Collapsed_Hovered"))
				.UncheckedPressedImage(FAppStyle::GetBrush("TreeArrow_Collapsed"))
				[
					SNew(STextBlock)
					.Text(INVTEXT("Add/Edit Channel"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SBox)
				.MaxDesiredWidth(328.f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
					.Padding(16.f, 3.f)
					[
						SNew(SVerticalBox)
						.Visibility_Lambda( [this]
						{
							return bAddNewVisible ? EVisibility::Visible : EVisibility::Collapsed;
						})
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							DetailsView->GetWidget().ToSharedRef()
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(2.f)
						[
							SNew(SButton)
							.Text_Lambda([this]
							{
								return AddEditButtonText;
							})
							.OnClicked(FOnClicked::CreateSP(this, &SVoxelChannelEditor::OnCreateEditChannelClicked))
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked_Lambda([this]
					{
						for (const auto& It : AllChannels)
						{
							ChannelsTreeWidget->SetItemExpansion(It.Value, true);
						}

						return FReply::Handled();
					})
					.Text(INVTEXT("Expand All"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked_Lambda([this]
					{
						ChannelsTreeWidget->ClearExpandedItems();
						return FReply::Handled();
					})
					.Text(INVTEXT("Collapse All"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.FillWidth(1.f)
				.Padding(5.f, 1.f)
				[
					SNew(SSearchBox)
					.HintText(INVTEXT("Search for Channel"))
					.OnTextChanged_Lambda([this](const FText& NewText)
					{
						LookupString = NewText;
						FilterChannels(false);
					})
				]
			]
			+ SVerticalBox::Slot()
			.MaxHeight(MaxHeight)
			[
				SAssignNew(ChannelsTreeBox, SBox)
				[
					SAssignNew(ChannelsTreeWidget, SChannelsTree)
					.TreeItemsSource(&ChannelItems)
					.OnGenerateRow(this, &SVoxelChannelEditor::OnGenerateRow)
					.OnGetChildren(this, &SVoxelChannelEditor::OnGetChildren)
					.SelectionMode(ESelectionMode::Single)
				]
			]
		]
	];

	FilterChannels(true);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVector2D SVoxelChannelEditor::ComputeDesiredSize(const float LayoutScaleMultiplier) const
{
	FVector2D WidgetSize = SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);

	const FVector2D TagTreeContainerSize = ChannelsTreeBox->GetDesiredSize();
	if (TagTreeContainerSize.Y < MaxHeight)
	{
		WidgetSize.Y += MaxHeight - TagTreeContainerSize.Y;
	}

	return WidgetSize;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<ITableRow> SVoxelChannelEditor::OnGenerateRow(TSharedPtr<FChannelNode> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SWidget> CheckBox = SNullWidget::NullWidget;
	TSharedPtr<SWidget> TypeBox = SNullWidget::NullWidget;

	if (!InItem->bIsCategory)
	{
		TOptional<FVoxelChannelDefinition> ChannelDef = GVoxelChannelManager->FindChannelDefinition(InItem->FullPath);
		if (ChannelDef.IsSet())
		{
			CheckBox =
				SNew(SCheckBox)
				.OnCheckStateChanged_Lambda([this, InItem](const ECheckBoxState NewState)
				{
					if (NewState != ECheckBoxState::Checked)
					{
						return;
					}

					OnChannelSelectedDelegate.ExecuteIfBound(InItem->FullPath);
					SelectedChannel = InItem->FullPath;
				})
				.IsChecked_Lambda([this, InItem]
				{
					return SelectedChannel == InItem->FullPath ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				});

			TypeBox =
				SNew(SImage)
				.Image(FVoxelGraphVisuals::GetPinIcon(ChannelDef->Type).GetIcon())
				.ColorAndOpacity(FVoxelGraphVisuals::GetPinColor(ChannelDef->Type))
				.DesiredSizeOverride(FVector2D(14.f, 14.f));
		}
	}

	const TSharedRef<SHorizontalBox> HorizontalBox =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		  .AutoWidth()
		  .HAlign(HAlign_Left)
		[
			CheckBox.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		  .AutoWidth()
		  .Padding(1.f)
		  .VAlign(VAlign_Center)
		[
			TypeBox.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		  .FillWidth(1.f)
		  .HAlign(HAlign_Left)
		  .VAlign(VAlign_Center)
		[
			SNew(SVoxelDetailText)
			.Font(FAppStyle::Get().GetFontStyle(InItem->bIsCategory ? "DetailsView.CategoryFontStyle" : "PropertyWindow.NormalFont"))
			.ColorAndOpacity(FLinearColor::White)
			.Text(FText::FromName(InItem->Name))
			.HighlightText_Lambda([this]
			{
				return LookupString;
			})
		]
		+ SHorizontalBox::Slot()
		  .AutoWidth()
		  .HAlign(HAlign_Right)
		[
			SNew(SButton)
			.ToolTipText(INVTEXT("Add sub channel"))
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.DesiredSizeScale(0.9f)
			.ContentPadding(2.f)
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(false)
			.OnClicked(FOnClicked::CreateSP(this, &SVoxelChannelEditor::OnAddSubChannelClicked, InItem))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

	if (InItem->bIsCategory)
	{
		if (!InItem->Parent &&
			Cast<UVoxelChannelRegistry>(InItem->Owner.Get()))
		{
			HorizontalBox->AddSlot()
			  .AutoWidth()
			  .HAlign(HAlign_Right)
			[
				SNew(SButton)
				.ToolTipText(INVTEXT("Browse to channel"))
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.DesiredSizeScale(0.9f)
				.ContentPadding(2.f)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsFocusable(false)
				.OnClicked(FOnClicked::CreateSP(this, &SVoxelChannelEditor::OnBrowseToChannel, InItem))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.BrowseContent"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
		}
	}
	else
	{
		HorizontalBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			SNew(SButton)
			.ToolTipText(INVTEXT("Edit channel"))
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.DesiredSizeScale(0.9f)
			.ContentPadding(2.f)
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(false)
			.OnClicked(FOnClicked::CreateSP(this, &SVoxelChannelEditor::OnEditChannelClicked, InItem))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Edit"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

		HorizontalBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			SNew(SButton)
			.ToolTipText(INVTEXT("Delete channel"))
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.DesiredSizeScale(0.9f)
			.ContentPadding(2.f)
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(false)
			.OnClicked(FOnClicked::CreateSP(this, &SVoxelChannelEditor::OnDeleteChannelClicked, InItem))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Delete"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
	}

	return
		SNew(STableRow<TSharedPtr<FChannelNode>>, OwnerTable)
		.Style(FAppStyle::Get(), "GameplayTagTreeView")
		[
			HorizontalBox
		];
}

void SVoxelChannelEditor::OnGetChildren(const TSharedPtr<FChannelNode> Item, TArray<TSharedPtr<FChannelNode>>& OutChildren) const
{
	for (const auto& It : Item->Channels)
	{
		OutChildren.Add(It.Value);
	}

	OutChildren.Sort([](const TSharedPtr<FChannelNode>& A, const TSharedPtr<FChannelNode>& B)
	{
		if (A->bIsCategory == B->bIsCategory)
		{
			return A->Name.ToString() < B->Name.ToString();
		}

		return A->bIsCategory;
	});
}

FReply SVoxelChannelEditor::OnAddSubChannelClicked(TSharedPtr<FChannelNode> Item)
{
	bAddNewVisible = true;
	AddEditButtonText = INVTEXT("Add new Channel");

	FName FullPath = FName(Item->FullPath.ToString() + ".MyChannel");
	while (AllChannels.Contains(FullPath))
	{
		FullPath.SetNumber(FullPath.GetNumber() + 1);
	}

	FVoxelChannelEditorDefinition* ChannelDefinition = StructOnScope->Get();
	ChannelDefinition->bEditChannel = false;
	ChannelDefinition->SaveLocation = Item->Owner;
	ChannelDefinition->Name = FName(FullPath.ToString().RightChop(FullPath.ToString().Find(".") + 1));

	DetailsView->GetDetailsView()->ForceRefresh();
	return FReply::Handled();
}

FReply SVoxelChannelEditor::OnEditChannelClicked(TSharedPtr<FChannelNode> Item)
{
	bAddNewVisible = true;
	AddEditButtonText = INVTEXT("Edit Channel");

	FVoxelChannelEditorDefinition* ChannelDefinition = StructOnScope->Get();
	ChannelDefinition->bEditChannel = true;
	ChannelDefinition->SaveLocation = Item->Owner;
	ChannelDefinition->Name = FName(Item->FullPath.ToString().RightChop(Item->FullPath.ToString().Find(".") + 1));

	TOptional<FVoxelChannelDefinition> ChannelDef = GVoxelChannelManager->FindChannelDefinition(Item->FullPath);
	if (ensure(ChannelDef.IsSet()))
	{
		ChannelDefinition->Type = ChannelDef->Type;
		ChannelDefinition->DefaultValue = ChannelDef->GetExposedDefaultValue();
	}

	DetailsView->GetDetailsView()->ForceRefresh();
	return FReply::Handled();
}

FReply SVoxelChannelEditor::OnDeleteChannelClicked(TSharedPtr<FChannelNode> Item)
{
	UObject* SaveLocation = Item->Owner.Get();
	if (!SaveLocation)
	{
		return FReply::Handled();
	}

	const FName FullPath = FName(Item->FullPath.ToString().RightChop(Item->FullPath.ToString().Find(".") + 1));
	{
		const FString TargetName = Cast<UVoxelSettings>(SaveLocation) ? "Project Settings" : SaveLocation->GetName() + " Asset";

		const EAppReturnType::Type DialogResult = FMessageDialog::Open(
			EAppMsgType::YesNo,
			EAppReturnType::No,
			FText::FromString("This action will edit " + TargetName + " to delete " + FullPath.ToString() + " channel.\nDo you wish to continue?"),
			INVTEXT("Delete Channel"));

		if (DialogResult == EAppReturnType::No)
		{
			return FReply::Handled();
		}
	}

	if (UVoxelSettings* Settings = Cast<UVoxelSettings>(SaveLocation))
	{
		FVoxelTransaction Transaction(Settings, "Delete Channel");
		Transaction.SetProperty(FindFPropertyChecked(UVoxelSettings, GlobalChannels));

		for (auto It = Settings->GlobalChannels.CreateIterator(); It; ++It)
		{
			if (It->Name == FullPath)
			{
				It.RemoveCurrentSwap();
				break;
			}
		}

		Settings->TryUpdateDefaultConfigFile();

		// Force save config now
		GConfig->Flush(false, GEngineIni);
	}
	else if (UVoxelChannelRegistry* Registry = Cast<UVoxelChannelRegistry>(SaveLocation))
	{
		FVoxelTransaction Transaction(Registry, "Delete Channel");
		Transaction.SetProperty(FindFPropertyChecked(UVoxelChannelRegistry, Channels));

		for (auto It = Registry->Channels.CreateIterator(); It; ++It)
		{
			if (It->Name == FullPath)
			{
				It.RemoveCurrentSwap();
				break;
			}
		}
	}
	else
	{
		ensure(false);
		return FReply::Handled();
	}

	if (SelectedChannel == Item->FullPath)
	{
		OnChannelSelectedDelegate.ExecuteIfBound(STATIC_FNAME("Project.Surface"));
	}

	return FReply::Handled();
}

FReply SVoxelChannelEditor::OnBrowseToChannel(TSharedPtr<FChannelNode> Item) const
{
	UObject* Object = Item->Owner.Get();
	if (!ensure(Cast<UVoxelChannelRegistry>(Object)))
	{
		return FReply::Handled();
	}

	FSlateApplication::Get().DismissAllMenus();

	TArray<UObject*> Assets{ Object };
	GEditor->SyncBrowserToObjects(Assets, true);

	return FReply::Handled();
}

FReply SVoxelChannelEditor::OnCreateEditChannelClicked()
{
	const FVoxelChannelEditorDefinition* ChannelDef = StructOnScope->Get();
	UObject* SaveLocation = ChannelDef->SaveLocation.Get();
	if (!SaveLocation)
	{
		return FReply::Handled();
	}

	{
		const FString TargetName = Cast<UVoxelSettings>(SaveLocation) ? "Project Settings" : SaveLocation->GetName() + " Asset";

		const EAppReturnType::Type DialogResult = FMessageDialog::Open(
			EAppMsgType::YesNo,
			EAppReturnType::No,
			FText::FromString("This action will edit " + TargetName + " to " + (ChannelDef->bEditChannel ? "update channel" : "create new channel") + ".\nDo you wish to continue?"),
			ChannelDef->bEditChannel ? INVTEXT("Edit Channel") : INVTEXT("Create new Channel"));

		if (DialogResult == EAppReturnType::No)
		{
			return FReply::Handled();
		}
	}

	FName NewChannelPath;
	const FVoxelChannelExposedDefinition NewChannel = *ChannelDef;
	if (UVoxelSettings* Settings = Cast<UVoxelSettings>(SaveLocation))
	{
		{
			FVoxelTransaction Transaction(Settings, "Add new Channel");
			Transaction.SetProperty(FindFPropertyChecked(UVoxelSettings, GlobalChannels));

			if (ChannelDef->bEditChannel)
			{
				for (FVoxelChannelExposedDefinition& Channel : Settings->GlobalChannels)
				{
					if (Channel.Name != ChannelDef->Name)
					{
						continue;
					}

					Channel.Type = ChannelDef->Type;
					Channel.DefaultValue = ChannelDef->DefaultValue;
					NewChannelPath = "Project." + Channel.Name;
					break;
				}
			}
			else
			{
				Settings->GlobalChannels.Add(NewChannel);
			}

			Settings->TryUpdateDefaultConfigFile();

			// Force save config now
			GConfig->Flush(false, GEngineIni);
		}

		if (!ChannelDef->bEditChannel &&
			ensure(Settings->GlobalChannels.Num() > 0))
		{
			NewChannelPath = "Project." + Settings->GlobalChannels[Settings->GlobalChannels.Num() - 1].Name;
		}
	}
	else if (UVoxelChannelRegistry* Registry = Cast<UVoxelChannelRegistry>(SaveLocation))
	{
		{
			FVoxelTransaction Transaction(Registry, "Add new Channel");
			Transaction.SetProperty(FindFPropertyChecked(UVoxelChannelRegistry, Channels));

			if (ChannelDef->bEditChannel)
			{
				for (FVoxelChannelExposedDefinition& Channel : Registry->Channels)
				{
					if (Channel.Name != ChannelDef->Name)
					{
						continue;
					}

					Channel.Type = ChannelDef->Type;
					Channel.DefaultValue = ChannelDef->DefaultValue;
					NewChannelPath = Registry->GetName() + "." + Channel.Name;
					break;
				}
			}
			else
			{
				Registry->Channels.Add(NewChannel);
			}
		}

		if (!ChannelDef->bEditChannel &&
			ensure(Registry->Channels.Num() > 0))
		{
			NewChannelPath = Registry->GetName() + "." + Registry->Channels[Registry->Channels.Num() - 1].Name;
		}
	}
	else
	{
		ensure(false);
		return FReply::Handled();
	}

	OnChannelSelectedDelegate.ExecuteIfBound(NewChannelPath);

	return FReply::Handled();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelChannelEditor::MapChannels()
{
	AllChannels = {};

	for (const UObject* Asset : GVoxelChannelManager->GetChannelAssets())
	{
		FString Prefix;
		const TArray<FVoxelChannelExposedDefinition>* ChannelsList;
		if (const UVoxelSettings* Settings = Cast<UVoxelSettings>(Asset))
		{
			ChannelsList = &Settings->GlobalChannels;
			Prefix = "Project.";
		}
		else if (const UVoxelChannelRegistry* Registry = Cast<UVoxelChannelRegistry>(Asset))
		{
			ChannelsList = &Registry->Channels;
			Prefix = Registry->GetName() + ".";
		}
		else
		{
			ensure(false);
			continue;
		}

		for (const FVoxelChannelExposedDefinition& Channel : *ChannelsList)
		{
			FString ChannelPath = Prefix + Channel.Name.ToString();

			TArray<FString> Path;
			ChannelPath.ParseIntoArray(Path, TEXT("."));

			if (!ensure(Path.Num() > 0))
			{
				continue;
			}

			FName RootPath = FName(Path[0]);
			TSharedPtr<FChannelNode> Node = AllChannels.FindRef(RootPath);
			if (!Node)
			{
				Node = MakeShared<FChannelNode>();
				Node->Name = RootPath;
				Node->FullPath = RootPath;
				Node->Owner = ConstCast(Asset);

				AllChannels.Add(RootPath, Node);
			}

			FString FullPath = RootPath.ToString() + ".";
			for (int32 Index = 1; Index < Path.Num(); Index++)
			{
				FullPath += Path[Index];

				FName CategoryPath = FName((Index < Path.Num() - 1 ? "CAT|" : "") + FullPath);
				TSharedPtr<FChannelNode> SubNode = AllChannels.FindRef(CategoryPath);
				if (!SubNode)
				{
					SubNode = MakeShared<FChannelNode>();
					SubNode->Name = FName(Path[Index]);
					SubNode->FullPath = CategoryPath;
					SubNode->Parent = Node;
					SubNode->Owner = ConstCast(Asset);

					AllChannels.Add(CategoryPath, SubNode);
				}

				Node = SubNode;

				FullPath += ".";
			}

			Node->FullPath = FName(ChannelPath);
			Node->bIsCategory = false;
		}
	}
}

void SVoxelChannelEditor::FilterChannels(const bool bOnlySelectedChannel)
{
	ChannelItems = {};

	const FString SearchString = LookupString.ToString();

	TArray<TSharedPtr<FChannelNode>> FilteredChannels;
	for (const auto& It : AllChannels)
	{
		It.Value->Channels = {};

		if (It.Value->Name.ToString().Contains(SearchString))
		{
			FilteredChannels.Add(It.Value);
		}
	}

	ChannelsTreeWidget->ClearExpandedItems();

	TSet<FName> AddedRootNodes;
	for (const TSharedPtr<FChannelNode>& ChannelNode : FilteredChannels)
	{
		TSharedPtr<FChannelNode> Node = ChannelNode;
		while (Node)
		{
			if (!bOnlySelectedChannel ||
				ChannelNode->FullPath == SelectedChannel)
			{
				ChannelsTreeWidget->SetItemExpansion(Node, true);
			}

			if (!Node->Parent)
			{
				if (!AddedRootNodes.Contains(Node->Name))
				{
					AddedRootNodes.Add(Node->Name);
					ChannelItems.Add(Node);
				}
				break;
			}

			Node->Parent->Channels.Add(Node->FullPath, Node);
			Node = Node->Parent;
		}
	}

	ChannelsTreeWidget->RequestTreeRefresh();
}

void SVoxelChannelEditor::CreateDetailsView()
{
	StructOnScope = MakeShared<TStructOnScope<FVoxelChannelEditorDefinition>>();
	StructOnScope->InitializeAs<FVoxelChannelEditorDefinition>();
	FVoxelChannelEditorDefinition* ChannelDef = StructOnScope->Get();
	ChannelDef->Type = FVoxelPinType::Make<float>();
	ChannelDef->DefaultValue = FVoxelPinValue(ChannelDef->Type);
	ChannelDef->SaveLocation = UVoxelSettings::StaticClass()->GetDefaultObject();

	FName Path = STATIC_FNAME("Project.MyChannel");
	while (AllChannels.Contains(Path))
	{
		Path.SetNumber(Path.GetNumber() + 1);
	}

	ChannelDef->Name = FName(Path.ToString().RightChop(Path.ToString().Find(".") + 1));

	FDetailsViewArgs Args;
	Args.bAllowSearch = false;
	Args.bShowOptions = false;
	Args.bHideSelectionTip = true;
	Args.bShowPropertyMatrixButton = false;
	Args.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	const FStructureDetailsViewArgs StructArgs;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyModule.CreateStructureDetailView(Args, StructArgs, StructOnScope);
	DetailsView->GetDetailsView()->SetDisableCustomDetailLayouts(false);
	DetailsView->GetDetailsView()->SetGenericLayoutDetailsDelegate(MakeLambdaDelegate([]() -> TSharedRef<IDetailCustomization>
	{
		return MakeVoxelShared<FVoxelChannelEditorDefinitionCustomization>();
	}));
	DetailsView->GetDetailsView()->ForceRefresh();

	DetailsView->GetDetailsView()->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& PropertyChangedEvent)
	{
		FVoxelChannelEditorDefinition* ChannelDefinition = StructOnScope->Get();
		if (!ensure(ChannelDefinition))
		{
			return;
		}

		if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STATIC(FVoxelChannelEditorDefinition, Type))
		{
			ChannelDefinition->Fixup();
		}
		else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STATIC(FVoxelChannelEditorDefinition, SaveLocation))
		{
			UObject* SaveLocation = ChannelDefinition->SaveLocation.Get();
			if (!ensure(SaveLocation))
			{
				return;
			}

			FString RootName = "Project";
			if (!Cast<UVoxelSettings>(SaveLocation))
			{
				RootName = SaveLocation->GetName();
			}

			FName NewPath = FName(RootName + "." + ChannelDefinition->Name);
			while (AllChannels.Contains(NewPath))
			{
				NewPath.SetNumber(NewPath.GetNumber() + 1);
			}

			ChannelDefinition->Name = FName(NewPath.ToString().RightChop(NewPath.ToString().Find(".") + 1));
		}
	});
}