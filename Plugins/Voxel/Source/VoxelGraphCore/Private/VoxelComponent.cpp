// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelComponent.h"
#include "VoxelRuntime.h"
#include "VoxelGraphInterface.h"
#include "VoxelParameterContainer.h"

UVoxelComponent::UVoxelComponent()
{
	ParameterContainer = CreateDefaultSubobject<UVoxelParameterContainer>("ParameterContainer");
	ParameterContainer->bAlwaysEnabled = true;
	ParameterContainer->OnProviderChanged.AddWeakLambda(this, [this]
	{
		if (IsRuntimeCreated())
		{
			QueueRecreate();
		}
	});

	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
}

UVoxelComponent::~UVoxelComponent()
{
	ensure(!Runtime.IsValid());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelComponent::OnRegister()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	Super::OnRegister();

	if (!IsRuntimeCreated())
	{
		QueueCreateRuntime();
	}
}

void UVoxelComponent::OnUnregister()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	if (IsRuntimeCreated())
	{
		DestroyRuntime();
	}

	Super::OnUnregister();
}

void UVoxelComponent::PostLoad()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	Super::PostLoad();

	if (Graph_DEPRECATED)
	{
		ensure(!ParameterContainer->Provider);
		ParameterContainer->Provider = Graph_DEPRECATED.Get();
		Graph_DEPRECATED = {};

		ParameterCollection_DEPRECATED.MigrateTo(*ParameterContainer);
	}
}

void UVoxelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	VOXEL_FUNCTION_COUNTER_LLM();

	// We don't want to tick the BP in preview
	if (GetWorld()->IsGameWorld())
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}

	if (bRuntimeCreateQueued &&
		FVoxelRuntime::CanBeCreated(false))
	{
		CreateRuntime();
	}

	if (bRuntimeRecreateQueued &&
		FVoxelRuntime::CanBeCreated(false))
	{
		bRuntimeRecreateQueued = false;

		DestroyRuntime();
		CreateRuntime();
	}

	if (Runtime)
	{
		Runtime->Tick();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void UVoxelComponent::PostEditUndo()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	Super::PostEditUndo();

	if (IsValid(this))
	{
		if (!IsRuntimeCreated())
		{
			QueueCreateRuntime();
		}
	}
	else
	{
		if (IsRuntimeCreated())
		{
			DestroyRuntime();
		}
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelComponent::QueueCreateRuntime()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	if (FVoxelRuntime::CanBeCreated(false))
	{
		CreateRuntime();
	}
	else
	{
		bRuntimeCreateQueued = true;
	}
}

void UVoxelComponent::CreateRuntime()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	if (!FVoxelRuntime::CanBeCreated(true))
	{
		VOXEL_MESSAGE(Error, "{0}: Cannot create runtime: not ready. See log for more info", this);
		return;
	}

	bRuntimeCreateQueued = false;

	if (IsRuntimeCreated())
	{
		return;
	}

	Runtime = FVoxelRuntime::Create(
		*this,
		*this,
		{},
		*ParameterContainer);
}

void UVoxelComponent::DestroyRuntime()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	// Clear RuntimeRecreate queue
	bRuntimeRecreateQueued = false;

	if (!IsRuntimeCreated())
	{
		return;
	}

	Runtime->Destroy();
	Runtime.Reset();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelGraphInterface* UVoxelComponent::GetGraph() const
{
	return ParameterContainer->GetTypedProvider<UVoxelGraphInterface>();
}

void UVoxelComponent::SetGraph(UVoxelGraphInterface* NewGraph)
{
	ParameterContainer->SetProvider(NewGraph);
}