// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelGraphInstance.h"
#include "VoxelParameterContainer.h"
#include "ContentBrowserModule.h"

VOXEL_RUN_ON_STARTUP_EDITOR(RegisterVoxelGraphMigration)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(FContentBrowserMenuExtender_SelectedAssets::CreateLambda([=](const TArray<FAssetData>& SelectedAssets)
	{
		const TSharedRef<FExtender> Extender = MakeVoxelShared<FExtender>();

		for (const FAssetData& SelectedAsset : SelectedAssets)
		{
			if (SelectedAsset.GetClass() != UVoxelGraphInstance::StaticClass())
			{
				return Extender;
			}
		}

		Extender->AddMenuExtension(
			"CommonAssetActions",
			EExtensionHook::Before,
			nullptr,
			FMenuExtensionDelegate::CreateLambda([=](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuEntry(
					INVTEXT("Enable all graph properties"),
					TAttribute<FText>(),
					FSlateIcon(NAME_None, NAME_None),
					FUIAction(FExecuteAction::CreateLambda([=]
					{
						for (const FAssetData& SelectedAsset : SelectedAssets)
						{
							UVoxelGraphInstance* GraphInstance = Cast<UVoxelGraphInstance>(SelectedAsset.GetAsset());
							if (!ensure(GraphInstance))
							{
								continue;
							}

							GraphInstance->Modify();

							for (auto& It : GraphInstance->ParameterContainer->ValueOverrides)
							{
								It.Value.bEnable = true;
							}
						}
					})));
			}));

		return Extender;
	}));
}