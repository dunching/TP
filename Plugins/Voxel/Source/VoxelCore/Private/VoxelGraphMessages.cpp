// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphMessages.h"
#include "EdGraph/EdGraph.h"
#include "Logging/TokenizedMessage.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Widgets/Docking/SDockTab.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

VOXELCORE_API FVoxelGraphMessages* GVoxelGraphMessages = nullptr;

FVoxelGraphMessages::FVoxelGraphMessages(const UEdGraph* Graph)
	: Graph(Graph)
{
	FVoxelMessages::OnClearNodeMessages.Broadcast(Graph);

	PreviousMessages = GVoxelGraphMessages;
	GVoxelGraphMessages = this;
}

FVoxelGraphMessages::~FVoxelGraphMessages()
{
	ensure(GVoxelGraphMessages == this);
	GVoxelGraphMessages = PreviousMessages;
}

void FVoxelGraphMessages::FMessageConsumer::LogMessage(const TSharedRef<FTokenizedMessage>& Message)
{
	if (Message->GetSeverity() == EMessageSeverity::Error)
	{
		*HasErrorPtr = true;
	}

#if WITH_EDITOR
	for (UObject* Object = ConstCast(Graph.Get()); Object; Object = Object->GetOuter())
	{
		if (IAssetEditorInstance* Editor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Object, false))
		{
			if (const TSharedPtr<FTabManager> TabManager = Editor->GetAssociatedTabManager())
			{
				if (TabManager->GetOwnerTab()->HasAnyUserFocus())
				{
					// Don't show a popup if the asset is opened
					// Always log
					LOG_VOXEL(Error, "%s", *Message->ToText().ToString());
					return;
				}
			}
		}
	}
#endif

	FVoxelMessages::LogMessage(Message);
}