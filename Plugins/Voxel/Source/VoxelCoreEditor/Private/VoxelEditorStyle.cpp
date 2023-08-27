// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEditorStyle.h"
#include "VoxelEditorMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

VOXEL_CONSOLE_COMMAND(
	ReinitializeStyle,
	"voxel.editor.ReinitializeStyle",
	"")
{
	FVoxelEditorStyle::ReinitializeStyle();
}

VOXEL_CONSOLE_COMMAND(
	ReloadTextures,
	"voxel.editor.ReloadTextures",
	"")
{
	FVoxelEditorStyle::ReloadTextures();
}

TSharedPtr<FVoxelStyleSet> FVoxelEditorStyle::VoxelEditorStyle = nullptr;
constexpr const TCHAR* GVoxelStyleSetName = TEXT("VoxelStyle");

FVoxelStyleSet::FVoxelStyleSet(const FName& InStyleSetName) : FSlateStyleSet(InStyleSetName)
{
	FVoxelStyleSet::SetContentRoot(FVoxelSystemUtilities::GetPlugin().GetBaseDir() / TEXT("Resources/EditorIcons"));
	FVoxelStyleSet::SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
}

void FVoxelStyleSet::InitModule(FVoxelStyleSet& Style, const FOnVoxelStyleReinitialize& Delegate)
{
	check(!FSlateStyleRegistry::FindSlateStyle(GVoxelStyleSetName));

#define CopyResources(Variable) \
	for (const auto& It : Style.Variable) \
	{ \
		Variable.Add(It.Key, It.Value); \
	} \
	Style.Variable.Empty();

	CopyResources(WidgetStyleValues);
	CopyResources(FloatValues);
	CopyResources(Vector2DValues);
	CopyResources(ColorValues);
	CopyResources(SlateColorValues);
	CopyResources(MarginValues);
	CopyResources(BrushResources);
	CopyResources(Sounds);
	CopyResources(FontInfoResources);
	CopyResources(DynamicBrushes);

#undef CopyResources

	Styles.Add(Delegate);
}

void FVoxelEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FVoxelEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

void FVoxelEditorStyle::Shutdown()
{
	Unregister();
	VoxelEditorStyle.Reset();
}

void FVoxelEditorStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

void FVoxelEditorStyle::ReinitializeStyle()
{
	TArray<FOnVoxelStyleReinitialize> Delegates = VoxelEditorStyle->Styles;

	Shutdown();
	VoxelEditorStyle = MakeVoxelShared<FVoxelStyleSet>(GVoxelStyleSetName);

	for (const FOnVoxelStyleReinitialize& Delegate : Delegates)
	{
		if (Delegate.IsBound())
		{
			VoxelEditorStyle->InitModule(*Delegate.Execute(), Delegate);
		}
	}

	Register();
}

const FVoxelStyleSet& FVoxelEditorStyle::Get()
{
	if (!VoxelEditorStyle.IsValid())
	{
		VoxelEditorStyle = MakeVoxelShared<FVoxelStyleSet>(GVoxelStyleSetName);
	}

	return *VoxelEditorStyle;
}

// Run this last, after all styles are registered
VOXEL_RUN_ON_STARTUP(RegisterVoxelEditorStyle, Editor, -999)
{
	FVoxelEditorStyle::Register();
}

VOXEL_INITIALIZE_STYLE(EditorBase)
{
	Set("VoxelIcon", new IMAGE_BRUSH("UIIcons/VoxelIcon_40x", CoreStyleConstants::Icon16x16));
	Set("VoxelEdMode", new IMAGE_BRUSH("UIIcons/VoxelIcon_40x", CoreStyleConstants::Icon40x40));
	Set("VoxelEdMode.Small", new IMAGE_BRUSH("UIIcons/VoxelIcon_40x", CoreStyleConstants::Icon16x16));
}