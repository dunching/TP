// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelFutureValue.h"
#include "VoxelGraphNodeRef.h"

struct FVoxelSpawnable;
class FVoxelRuntime;
class FVoxelQueryContext;

struct VOXELGRAPHCORE_API FVoxelMergeSpawnableRef
{
public:
	TWeakObjectPtr<const UObject> RuntimeProvider;
	// Node actually spawning the Spawnable, eg SpawnChunks3D
	FVoxelNodePath NodePath;
	FVoxelInstancedStruct NodeData;

public:
	FVoxelMergeSpawnableRef() = default;
	explicit FVoxelMergeSpawnableRef(const FVoxelQueryContext& SpawnContext);

	FORCEINLINE UWorld* GetWorld() const
	{
		checkVoxelSlow(IsInGameThread());
		if (const UObject* Object = RuntimeProvider.Get())
		{
			return Object->GetWorld();
		}
		return nullptr;
	}
	FORCEINLINE bool IsValid() const
	{
		return RuntimeProvider.IsValid();
	}

	TSharedPtr<FVoxelRuntime> GetRuntime(FString* OutError = nullptr) const;
	TVoxelFutureValue<FVoxelSpawnable> Resolve(FString* OutError = nullptr) const;

	FORCEINLINE bool operator==(const FVoxelMergeSpawnableRef& Other) const
	{
		return
			RuntimeProvider == Other.RuntimeProvider &&
			NodePath == Other.NodePath &&
			NodeData == Other.NodeData;
	}
	FORCEINLINE friend uint32 GetTypeHash(const FVoxelMergeSpawnableRef& Ref)
	{
		return FVoxelUtilities::MurmurHashMulti(
			GetTypeHash(Ref.RuntimeProvider),
			GetTypeHash(Ref.NodePath),
			Ref.NodeData.GetPropertyHash());
	}
};

struct VOXELGRAPHCORE_API FVoxelSpawnableRef
{
public:
	FVoxelMergeSpawnableRef MergeSpawnableRef;
	// Node creating the Spawnable, eg SpawnMesh
	FVoxelNodePath NodePath;

	TSharedPtr<FVoxelSpawnable> Resolve(FString* OutError = nullptr) const;
	bool NetSerialize(FArchive& Ar, UPackageMap& Map);

public:
	FORCEINLINE UWorld* GetWorld() const
	{
		return MergeSpawnableRef.GetWorld();
	}
	FORCEINLINE bool IsValid() const
	{
		return MergeSpawnableRef.IsValid();
	}
	FORCEINLINE bool operator==(const FVoxelSpawnableRef& Other) const
	{
		return
			MergeSpawnableRef == Other.MergeSpawnableRef &&
			NodePath == Other.NodePath;
	}
	FORCEINLINE friend uint32 GetTypeHash(const FVoxelSpawnableRef& Ref)
	{
		return FVoxelUtilities::MurmurHashMulti(
			GetTypeHash(Ref.MergeSpawnableRef),
			GetTypeHash(Ref.NodePath));
	}
};

template<>
struct VOXELGRAPHCORE_API TVoxelMessageArgProcessor<FVoxelSpawnableRef>
{
	static void ProcessArg(FVoxelMessageBuilder& Builder, const FVoxelSpawnableRef& Ref);
};