// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphInterfaceThumbnailRenderer.h"
#include "VoxelActor.h"
#include "VoxelRuntime.h"
#include "SceneView.h"
#include "Engine/Texture2D.h"

DEFINE_VOXEL_THUMBNAIL_RENDERER(UVoxelGraphInterfaceThumbnailRenderer, UVoxelGraphInterface);

VOXEL_CONSOLE_VARIABLE(
	VOXELGRAPHEDITOR_API, bool, GVoxelEnableGraphThumbnails, true,
	"voxel.EnableGraphThumbnails",
	"");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelGraphThumbnailScene::FVoxelGraphThumbnailScene()
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.bNoFail = true;
	SpawnInfo.bDeferConstruction = true;
	SpawnInfo.ObjectFlags = RF_Transient;
	SpawnInfo.Name = MakeUniqueObjectName(GetWorld(), AVoxelActor::StaticClass(), "VoxelThumbnailActor");

	VoxelActor = GetWorld()->SpawnActor<AVoxelActor>(SpawnInfo);
	VoxelActor->bCreateRuntimeOnBeginPlay = false;
	VoxelActor->bCreateRuntimeOnConstruction_EditorOnly = false;
	VoxelActor->FinishSpawning(FTransform::Identity);
}

FBoxSphereBounds FVoxelGraphThumbnailScene::GetBounds() const
{
	if (!ensure(VoxelActor.IsValid()))
	{
		return {};
	}

	const TSharedPtr<FVoxelRuntime> Runtime = VoxelActor->GetRuntime();
	if (!ensure(Runtime))
	{
		return {};
	}

	const FVoxelOptionalBox Bounds = Runtime->GetBounds();
	if (!Bounds.IsValid())
	{
		return {};
	}

	return Bounds.GetBox().ToFBox();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelGraphInterfaceThumbnailRenderer::BeginDestroy()
{
	ThumbnailScene.Reset();

	Super::BeginDestroy();
}

void UVoxelGraphInterfaceThumbnailRenderer::Draw(
	UObject* Object,
	const int32 X,
	const int32 Y,
	const uint32 Width,
	const uint32 Height,
	FRenderTarget* Viewport,
	FCanvas* Canvas,
	const bool bAdditionalViewFamily)
{
	VOXEL_FUNCTION_COUNTER();

	if (!GVoxelEnableGraphThumbnails)
	{
		return;
	}

	const TSharedRef<FVoxelMessageSinkConsumer> MessageConsumer = MakeVoxelShared<FVoxelMessageSinkConsumer>();
	const FVoxelScopedMessageConsumer ScopedMessageConsumer = FVoxelScopedMessageConsumer(MessageConsumer);

	if (!ThumbnailScene)
	{
		ThumbnailScene = MakeVoxelShared<FVoxelGraphThumbnailScene>();
	}
	if (!ensure(ThumbnailScene->VoxelActor.IsValid()))
	{
		return;
	}

	ensure(!ThumbnailScene->VoxelActor->IsRuntimeCreated());
	ThumbnailScene->VoxelActor->SetGraph(CastChecked<UVoxelGraphInterface>(Object));
	ThumbnailScene->VoxelActor->CreateRuntime();

	ON_SCOPE_EXIT
	{
		ThumbnailScene->VoxelActor->SetGraph(nullptr);
		ThumbnailScene->VoxelActor->DestroyRuntime();
	};

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
		.SetTime(GetTime())
		.SetAdditionalViewFamily(bAdditionalViewFamily));

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	FSceneView* View = ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height);

	RenderViewFamily(Canvas, &ViewFamily, View);
}