// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSculptEdMode.h"
#include "VoxelSculptToolkit.h"
#include "VoxelRuntime.h"
#include "VoxelTaskGroup.h"
#include "VoxelDependency.h"
#include "VoxelBufferUtilities.h"
#include "VoxelParameterContainer.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "VoxelPositionQueryParameter.h"
#include "SculptVolume/VoxelSculptVolumeStorage.h"
#include "SculptVolume/VoxelSculptVolumeStorageData.h"
#include "EdModeInteractiveToolsContext.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"

DEFINE_VOXEL_COMMANDS(FVoxelSculptCommands);

void FVoxelSculptCommands::RegisterCommands()
{
	VOXEL_UI_COMMAND(Select, "Select", "Select", EUserInterfaceActionType::ToggleButton, {});
	VOXEL_UI_COMMAND(Sculpt, "Sculpt", "Sculpt", EUserInterfaceActionType::ToggleButton, {});
}

void UVoxelSelectTool::Setup()
{
	Super::Setup();

	Properties = NewObject<UVoxelSelectToolProperties>(this);
	Properties->RestoreProperties(this);
	AddToolPropertySource(Properties);
}

void UVoxelSelectTool::Shutdown(EToolShutdownType ShutdownType)
{
	Properties->SaveProperties(this);
}

FInputRayHit UVoxelSelectTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	FViewport* FocusedViewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	if (!FocusedViewport)
	{
		return {};
	}

	HHitProxy* HitProxy = FocusedViewport->GetHitProxy(ClickPos.ScreenPosition.X, ClickPos.ScreenPosition.Y);
	if (!HitProxy)
	{
		return {};
	}

	HActor* ActorHitProxy = HitProxyCast<HActor>(HitProxy);
	if (!ActorHitProxy)
	{
		return {};
	}

	UE_DEBUG_BREAK();
	return {};
}

void UVoxelSelectTool::OnClicked(const FInputDeviceRay& ClickPos)
{

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelSculptTool::Setup()
{
	VOXEL_FUNCTION_COUNTER();

	Super::Setup();

	{
		UMouseHoverBehavior* Behavior = NewObject<UMouseHoverBehavior>();
		Behavior->Initialize(this);
		AddInputBehavior(Behavior);
	}
	{
		UClickDragInputBehavior* Behavior = NewObject<UClickDragInputBehavior>();
		Behavior->Initialize(this);
		AddInputBehavior(Behavior);
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.bDeferConstruction = true;
	SpawnParameters.ObjectFlags = RF_Transient;
	PreviewActor = GetWorld()->SpawnActor<AVoxelPreviewActor>(SpawnParameters);
	PreviewActor->SetActorLabel("VoxelSculptActor");
	PreviewActor->bCreateRuntimeOnBeginPlay = false;
	PreviewActor->DefaultRuntimeParameters.Add(NewSurfaceParameter);
	PreviewActor->DefaultRuntimeParameters.Add<FVoxelRuntimeParameter_DisableCollision>();

	FString GraphString;
	if (GConfig->GetString(
		TEXT("VoxelSculptTool"),
		TEXT("Graph"),
		GraphString,
		GEditorPerProjectIni))
	{
		FVoxelObjectUtilities::PropertyFromText_InContainer(
			FindFPropertyChecked(UVoxelParameterContainer, Provider),
			GraphString,
			PreviewActor->ParameterContainer);
	}

	FString ParameterCollectionString;
	if (GConfig->GetString(
		TEXT("VoxelSculptTool"),
		TEXT("ParameterCollection"),
		ParameterCollectionString,
		GEditorPerProjectIni))
	{
		FVoxelObjectUtilities::PropertyFromText_InContainer(
			FindFPropertyChecked(UVoxelParameterContainer, Provider),
			ParameterCollectionString,
			PreviewActor->ParameterContainer);
	}

	PreviewActor->FinishSpawning(FTransform::Identity);

	AddToolPropertySource(PreviewActor);

	const auto SetSculptVolume = [this](AVoxelSculptVolume* SculptVolume)
	{
		if (!ensure(PreviewActor))
		{
			return;
		}

		PreviewActor->DefaultRuntimeParameters.Remove<FVoxelRuntimeParameter_SculptVolumeStorage>();
		PreviewActor->SculptVolume = SculptVolume;

		if (SculptVolume)
		{
			const TSharedRef<FVoxelRuntimeParameter_SculptVolumeStorage> Parameter = SculptVolume->GetSculptVolumeParameter();
			Parameter->SurfaceToWorldOverride = FVoxelTransformRef::Make(*SculptVolume);
			PreviewActor->DefaultRuntimeParameters.Add(Parameter);
			PreviewActor->QueueRecreate();
		}
		else
		{
			PreviewActor->DestroyRuntime();
		}
	};
	const auto OnSelectionChanged = [this, SetSculptVolume](UObject*)
	{
		for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
		{
			if (AVoxelSculptVolume* Volume = Cast<AVoxelSculptVolume>(*It))
			{
				SetSculptVolume(Volume);
				break;
			}
		}

	};
	USelection::SelectionChangedEvent.AddWeakLambda(this, OnSelectionChanged);
	OnSelectionChanged(nullptr);

	if (!PreviewActor->SculptVolume)
	{
		for (AVoxelSculptVolume* Volume : TActorRange<AVoxelSculptVolume>(GetWorld()))
		{
			SetSculptVolume(Volume);
			break;
		}
	}
}

void UVoxelSculptTool::Shutdown(EToolShutdownType ShutdownType)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(PreviewActor))
	{
		return;
	}

	GConfig->SetString(
		TEXT("VoxelSculptTool"),
		TEXT("Graph"),
		*FVoxelObjectUtilities::PropertyToText_InContainer(
			FindFPropertyChecked(UVoxelParameterContainer, Provider),
			PreviewActor->ParameterContainer),
		GEditorPerProjectIni);

	GConfig->SetString(
		TEXT("VoxelSculptTool"),
		TEXT("ParameterCollection"),
		*FVoxelObjectUtilities::PropertyToText_InContainer(
			FindFPropertyChecked(UVoxelParameterContainer, Provider),
			PreviewActor->ParameterContainer),
		GEditorPerProjectIni);

	PreviewActor->Destroy();

	USelection::SelectionChangedEvent.RemoveAll(this);
}

void UVoxelSculptTool::OnTick(const float DeltaTime)
{
	if (!bIsEditing)
	{
		return;
	}

	if (LastRay)
	{
		UpdatePosition(LastRay.GetValue());
	}

	(void)DoEdit();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FInputRayHit UVoxelSculptTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	FInputRayHit Hit;
	Hit.bHit = true;
	return Hit;
}

bool UVoxelSculptTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	UpdatePosition(DevicePos);
	return true;
}

FInputRayHit UVoxelSculptTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FInputRayHit Result;
	Result.bHit = DoEdit();
	return Result;
}

void UVoxelSculptTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	UpdatePosition(PressPos);
	DoEdit();

	ensure(!bIsEditing);
	bIsEditing = true;
}

void UVoxelSculptTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	UpdatePosition(DragPos);
}

void UVoxelSculptTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	ensure(bIsEditing);
	bIsEditing = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelSculptTool::UpdatePosition(const FInputDeviceRay& Position)
{
	LastRay = Position;

	FHitResult HitResult;
	if (!GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Position.WorldRay.Origin,
		Position.WorldRay.Origin + Position.WorldRay.Direction * 1e6,
		ECC_EngineTraceChannel6))
	{
		return;
	}

	if (!ensure(PreviewActor) ||
		!HitResult.bBlockingHit)
	{
		return;
	}

	PreviewActor->SetActorLocation(HitResult.Location);
}

bool UVoxelSculptTool::DoEdit() const
{
	if (!ensure(PreviewActor) ||
		!PreviewActor->GetGraph() ||
		!PreviewActor->SculptVolume ||
		!PreviewActor->IsRuntimeCreated())
	{
		VOXEL_MESSAGE(Error, "No Sculpt Volume selected");
		return false;
	}

	AVoxelSculptVolume* SculptVolume = PreviewActor->SculptVolume;
	if (!SculptVolume->IsRuntimeCreated())
	{
		VOXEL_MESSAGE(Error, "{0}: Sculpt volume not created", SculptVolume);
		return false;
	}

	const TSharedPtr<FVoxelSetSculptVolumeSurfaceExecNodeRuntime> NodeRuntime = NewSurfaceParameter->WeakRuntime.Pin();
	if (!NodeRuntime)
	{
		VOXEL_MESSAGE(Error, "{0}: No Set Sculpt Volume Surface node", PreviewActor->GetGraph());
		return false;
	}

	const TOptional<FVoxelBox> OptionalLocalBounds = FVoxelTaskGroup::TryRunSynchronously(NodeRuntime->GetContext(), [&]
	{
		const FVoxelQuery Query = FVoxelQuery::Make(
			FVoxelQueryContext::Make(SculptVolume->GetRuntime()->GetRuntimeInfoRef().AsShared()),
			MakeVoxelShared<FVoxelQueryParameters>(),
			FVoxelDependencyTracker::Create("VoxelSculpt"))
			.MakeNewQuery(NodeRuntime->GetContext());

		return NodeRuntime->GetNodeRuntime().Get(NodeRuntime->Node.BoundsPin, Query);
	});

	if (!ensure(OptionalLocalBounds))
	{
		return false;
	}

	const FVoxelBox LocalBounds = OptionalLocalBounds->TransformBy(
		PreviewActor->ActorToWorld().ToMatrixWithScale() *
		SculptVolume->ActorToWorld().ToInverseMatrixWithScale());

	// MakeMultipleOf to not have to handle partial chunk updates & querying the source data manually again
	const FVoxelIntBox IntBounds = FVoxelIntBox::FromFloatBox_WithPadding(LocalBounds)
		.MakeMultipleOfBigger(FVoxelSculptVolumeStorageData::ChunkSize);

	if (IntBounds.Count_LargeBox() > 1024 * 1024)
	{
		VOXEL_MESSAGE(Error, "Cannot apply tool: more than 1M voxels would be computed");
		return false;
	}

	const TSharedRef<FVoxelQueryParameters> QueryParameters = MakeVoxelShared<FVoxelQueryParameters>();
	QueryParameters->Add<FVoxelLODQueryParameter>().LOD = 0;
	QueryParameters->Add<FVoxelPositionQueryParameter>().InitializeGrid3D(FVector3f(IntBounds.Min), 1.f, IntBounds.Size());

	const TOptional<FVoxelFloatBuffer> SurfaceDistances = FVoxelTaskGroup::TryRunSynchronously(NodeRuntime->GetContext(), [&]
	{
		const FVoxelQuery Query = FVoxelQuery::Make(
			FVoxelQueryContext::Make(SculptVolume->GetRuntime()->GetRuntimeInfoRef().AsShared()),
			QueryParameters,
			FVoxelDependencyTracker::Create("VoxelSculpt"))
			.MakeNewQuery(NodeRuntime->GetContext());

		const TVoxelFutureValue<FVoxelSurface> Surface = NodeRuntime->GetNodeRuntime().Get(NodeRuntime->Node.SurfacePin, Query);
		return
			MakeVoxelTask()
			.Dependency(Surface)
			.Execute<FVoxelFloatBuffer>([=]
			{
				return Surface.Get_CheckCompleted().GetDistance(Query);
			});
	});

	if (!ensure(SurfaceDistances))
	{
		return false;
	}

	if (SurfaceDistances->Num() != 1 &&
		SurfaceDistances->Num() != IntBounds.Count_SmallBox())
	{
		VOXEL_MESSAGE(Error, "{0}: Surface has a different buffer size than Positions", NodeRuntime.Get());
		return false;
	}

	TVoxelArray<float> Distances;
	FVoxelUtilities::SetNumFast(Distances, IntBounds.Count_SmallBox());

	FVoxelBufferUtilities::UnpackData(
		SurfaceDistances->GetStorage(),
		Distances,
		IntBounds.Size());

	const TSharedRef<FVoxelSculptVolumeStorageData> Data = SculptVolume->GetData();
	Data->SetDistances(IntBounds, ReinterpretCastVoxelArrayView<float>(Distances));
	SculptVolume->MarkPackageDirty();
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelSculptEdMode::UVoxelSculptEdMode()
{
	SettingsClass = UVoxelSculptEdModeSettings::StaticClass();

	Info = FEditorModeInfo(
		"VoxelSculptEdMode",
		INVTEXT("Voxel Sculpt"),
		FSlateIcon(FVoxelEditorStyle::GetStyleSetName(), "VoxelIcon"),
		true);
}

void UVoxelSculptEdMode::Enter()
{
	Super::Enter();

	const FVoxelSculptCommands& Commands = FVoxelSculptCommands::Get();

	RegisterTool(
		Commands.Select,
		GetClassName<UVoxelSelectTool>(),
		NewObject<UVoxelSelectToolBuilder>(this));

	RegisterTool(
		Commands.Sculpt,
		GetClassName<UVoxelSculptTool>(),
		NewObject<UVoxelSculptToolBuilder>(this));

	GetInteractiveToolsContext()->StartTool(GetClassName<UVoxelSculptTool>());
}

void UVoxelSculptEdMode::Exit()
{
	Super::Exit();
}

void UVoxelSculptEdMode::CreateToolkit()
{
	Toolkit = MakeVoxelShared<FVoxelSculptToolkit>();
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UVoxelSculptEdMode::GetModeCommands() const
{
	const FVoxelSculptCommands& Commands = FVoxelSculptCommands::Get();

	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Result;
	Result.Add("Tools", {
		Commands.Select,
		Commands.Sculpt
	});
	return Result;
}