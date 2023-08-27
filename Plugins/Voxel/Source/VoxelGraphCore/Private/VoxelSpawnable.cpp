// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSpawnable.h"

FVoxelSpawnable::~FVoxelSpawnable()
{
	ensure(bIsDestroyed || !bIsCreated);
}

void FVoxelSpawnable::CallCreate_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();
	check(RuntimeInfo);

	ensure(!bIsCreated);
	ensure(!bIsDestroyed);
	bIsCreated = true;

	Create_AnyThread();
}

void FVoxelSpawnable::CallDestroy_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();
	check(RuntimeInfo);

	ensure(bIsCreated);
	ensure(!bIsDestroyed);
	bIsDestroyed = true;

	Destroy_AnyThread();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnable::InitializeInternal(
	const FVoxelQuery& Query,
	const FVoxelGraphNodeRef& Node)
{
	PrivateNode = Node;

	const FVoxelMergeSpawnableRefQueryParameter* Parameter = Query.GetParameters().Find<FVoxelMergeSpawnableRefQueryParameter>();
	if (ensure(Parameter))
	{
		PrivateSpawnableRef.MergeSpawnableRef = Parameter->MergeSpawnableRef;
		PrivateSpawnableRef.NodePath = Query.GetContext().Path;
		PrivateSpawnableRef.NodePath.NodeRefs.Add(Node);
	}

	RuntimeInfo = Query.GetContext().RuntimeInfo;
}

TSharedRef<const FVoxelQueryParameters> MakeVoxelSpawnablePointQueryParameters(
	const TSharedRef<const FVoxelQueryContext>& QueryContext,
	const TSharedRef<const FVoxelQueryParameters>& QueryParameters,
	const FVoxelGraphNodeRef& Node)
{
	const TSharedRef<FVoxelQueryParameters> NewParameters = QueryParameters->Clone();
	ensure(!NewParameters->Find<FVoxelSpawnableRefQueryParameter>());

	FVoxelSpawnableRef SpawnableRef;

	const FVoxelMergeSpawnableRefQueryParameter* Parameter = QueryParameters->Find<FVoxelMergeSpawnableRefQueryParameter>();
	if (ensure(Parameter))
	{
		SpawnableRef.MergeSpawnableRef = Parameter->MergeSpawnableRef;
		SpawnableRef.NodePath = QueryContext->Path;
		SpawnableRef.NodePath.NodeRefs.Add(Node);
	}

	NewParameters->Add<FVoxelSpawnableRefQueryParameter>().SpawnableRef = SpawnableRef;

	return NewParameters;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGameThreadSpawnable::Create_AnyThread()
{
	FVoxelUtilities::RunOnGameThread([This = SharedThis(this)]
	{
		VOXEL_SCOPE_COUNTER("Create_GameThread");
		This->Create_GameThread();
	});
}

void FVoxelGameThreadSpawnable::Destroy_AnyThread()
{
	FVoxelUtilities::RunOnGameThread([This = SharedThis(this)]
	{
		VOXEL_SCOPE_COUNTER("Destroy_GameThread");
		This->Destroy_GameThread();
	});
}