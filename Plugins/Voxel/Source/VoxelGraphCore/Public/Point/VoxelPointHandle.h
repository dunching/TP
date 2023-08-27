// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelSpawnableRef.h"
#include "Point/VoxelPointId.h"
#include "VoxelPointHandle.generated.h"

USTRUCT(BlueprintType)
struct VOXELGRAPHCORE_API FVoxelPointHandle
{
	GENERATED_BODY()

public:
	FVoxelSpawnableRef SpawnableRef;
	FVoxelPointId PointId;

public:
	FORCEINLINE UWorld* GetWorld() const
	{
		return SpawnableRef.GetWorld();
	}
	FORCEINLINE bool IsValid() const
	{
		return
			SpawnableRef.IsValid() &&
			PointId.IsValid();
	}
	FORCEINLINE bool operator==(const FVoxelPointHandle& Other) const
	{
		return
			SpawnableRef == Other.SpawnableRef &&
			PointId == Other.PointId;
	}
	FORCEINLINE friend uint32 GetTypeHash(const FVoxelPointHandle& Handle)
	{
		return
			GetTypeHash(Handle.SpawnableRef) ^
			GetTypeHash(Handle.PointId);
	}

public:
	TSharedPtr<FVoxelRuntime> GetRuntime(FString* OutError = nullptr) const;
	bool GetAttributes(
		TVoxelMap<FName, FVoxelPinValue>& OutAttributes,
		FString* OutError = nullptr) const;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

private:
	bool NetSerializeImpl(FArchive& Ar, UPackageMap& Map);
};

template<>
struct TStructOpsTypeTraits<FVoxelPointHandle> : TStructOpsTypeTraitsBase2<FVoxelPointHandle>
{
	enum
	{
		WithNetSerializer = true,
	};
};