// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSpawnableRef.h"
#include "VoxelQuery.h"
#include "VoxelRuntime.h"
#include "VoxelExecNode.h"
#include "VoxelSpawnable.h"
#include "VoxelRuntimeProvider.h"
#include "VoxelMergeSpawnableNode.h"

FVoxelMergeSpawnableRef::FVoxelMergeSpawnableRef(const FVoxelQueryContext& Context)
{
	RuntimeProvider = Context.RuntimeInfo->GetInstance();
	NodePath = Context.Path;
}

TSharedPtr<FVoxelRuntime> FVoxelMergeSpawnableRef::GetRuntime(FString* OutError) const
{
	VOXEL_FUNCTION_COUNTER();

	const UObject* RuntimeProviderObject = RuntimeProvider.Get();
	if (!RuntimeProviderObject)
	{
		if (OutError)
		{
			*OutError = "Invalid RuntimeProvider";
		}
		return {};
	}

	const IVoxelRuntimeProvider* RuntimeProviderInterface = Cast<IVoxelRuntimeProvider>(RuntimeProviderObject);
	if (!ensure(RuntimeProviderInterface))
	{
		if (OutError)
		{
			*OutError = "Invalid RuntimeProvider";
		}
		return {};
	}

	const TSharedPtr<FVoxelRuntime> Runtime = RuntimeProviderInterface->GetRuntime();
	if (!Runtime)
	{
		if (OutError)
		{
			*OutError = "Runtime is destroyed";
		}
		return {};
	}

	return Runtime;
}

TVoxelFutureValue<FVoxelSpawnable> FVoxelMergeSpawnableRef::Resolve(FString* OutError) const
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedPtr<FVoxelRuntime> Runtime = GetRuntime(OutError);
	if (!Runtime)
	{
		return {};
	}

	const TVoxelFutureValue<FVoxelSpawnable> Spawnable = Runtime->GetNodeRuntime().GenerateSpawnable(*this);
	if (!Spawnable.IsValid())
	{
		if (OutError)
		{
			*OutError = "Could not find matching Spawnable";
		}
		return {};
	}

	return Spawnable;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FVoxelSpawnable> FVoxelSpawnableRef::Resolve(FString* OutError) const
{
	const TVoxelFutureValue<FVoxelSpawnable> MergeSpawnable = MergeSpawnableRef.Resolve(OutError);
	if (!MergeSpawnable.IsValid())
	{
		return {};
	}

	if (!MergeSpawnable.IsComplete())
	{
		VOXEL_MESSAGE(Error, "Spawnable is not ready yet");
		return nullptr;
	}

	if (MergeSpawnable.Get_CheckCompleted().GetStruct() != FVoxelMergedSpawnable::StaticStruct())
	{
		ensure(MergeSpawnable.Get_CheckCompleted().GetSpawnableRef().MergeSpawnableRef == MergeSpawnableRef);
		return ConstCast(MergeSpawnable.GetShared_CheckCompleted());
	}

	for (const TSharedPtr<FVoxelSpawnable>& OtherSpawnable : CastChecked<FVoxelMergedSpawnable>(MergeSpawnable.Get_CheckCompleted()).Spawnables)
	{
		if (OtherSpawnable->GetSpawnableRef().NodePath == NodePath)
		{
			ensure(OtherSpawnable->GetSpawnableRef().MergeSpawnableRef == MergeSpawnableRef);
			return OtherSpawnable.ToSharedRef();
		}
	}

	ensureVoxelSlow(false);
	return nullptr;
}

bool FVoxelSpawnableRef::NetSerialize(FArchive& Ar, UPackageMap& Map)
{
	VOXEL_FUNCTION_COUNTER();

	if (Ar.IsSaving())
	{
		ensure(IsValid());

		{
			UObject* Object = ConstCast(MergeSpawnableRef.RuntimeProvider.Get());
			ensure(Object);

			if (!ensure(Map.SerializeObject(Ar, UObject::StaticClass(), Object)))
			{
				return false;
			}
		}

		return
			ensure(MergeSpawnableRef.NodePath.NetSerialize(Ar, Map)) &&
			ensure(MergeSpawnableRef.NodeData.NetSerialize(Ar, Map)) &&
			ensure(NodePath.NetSerialize(Ar, Map));
	}
	else if (Ar.IsLoading())
	{
		{
			UObject* Object = nullptr;
			if (!ensure(Map.SerializeObject(Ar, UObject::StaticClass(), Object)) ||
				!ensure(Object))
			{
				return false;
			}

			MergeSpawnableRef.RuntimeProvider = Object;
		}

		return
			ensure(MergeSpawnableRef.NodePath.NetSerialize(Ar, Map)) &&
			ensure(MergeSpawnableRef.NodeData.NetSerialize(Ar, Map)) &&
			ensure(NodePath.NetSerialize(Ar, Map));
	}
	else
	{
		ensure(false);
		return false;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void TVoxelMessageArgProcessor<FVoxelSpawnableRef>::ProcessArg(FVoxelMessageBuilder& Builder, const FVoxelSpawnableRef& Ref)
{
	const TSharedRef<FVoxelMessageBuilder> ChildBuilder = MakeVoxelShared<FVoxelMessageBuilder>(Builder.Severity, "Instance: {0} Callstack: {1}");
	FVoxelMessageArgProcessor::ProcessArg(*ChildBuilder, Ref.MergeSpawnableRef.RuntimeProvider);
	FVoxelMessageArgProcessor::ProcessArg(*ChildBuilder, Ref.NodePath.NodeRefs);
	FVoxelMessageArgProcessor::ProcessArg(Builder, ChildBuilder);
}