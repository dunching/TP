// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelBufferBuilder.h"
#include "Point/VoxelPointHandle.h"

class FVoxelDependency;

class VOXELGRAPHCORE_API FVoxelPointStorageSpawnableData
{
public:
	const TSharedRef<FVoxelDependency> Dependency;
	mutable FVoxelCriticalSection CriticalSection;
	FVoxelPointIdBufferStorage NewPointIds;

	struct FAttribute
	{
		TVoxelMap<FVoxelPointId, int32> PointIdToIndex;
		TSharedPtr<FVoxelBufferBuilder> Buffer;
	};
	TVoxelMap<FName, TSharedPtr<FAttribute>> NameToAttributeOverride;

	explicit FVoxelPointStorageSpawnableData(const TSharedRef<FVoxelDependency>& Dependency)
		: Dependency(Dependency)
	{
	}
};

class VOXELGRAPHCORE_API FVoxelPointStorageData : public TSharedFromThis<FVoxelPointStorageData>
{
public:
	const FName AssetName;
	const TSharedRef<FVoxelDependency> Dependency;

	explicit FVoxelPointStorageData(FName AssetName);

	void ClearData();
	void Serialize(FArchive& Ar);

	TSharedPtr<const FVoxelPointStorageSpawnableData> FindSpawnableData(const FVoxelSpawnableRef& SpawnableRef) const;

	bool SetPointAttribute(
		const FVoxelPointHandle& Handle,
		FName Name,
		const FVoxelPinType& Type,
		const FVoxelPinValue& Value,
		FString* OutError = nullptr);

private:
	mutable FVoxelCriticalSection CriticalSection;
	TVoxelMap<FVoxelSpawnableRef, TSharedPtr<FVoxelPointStorageSpawnableData>> SpawnableRefToSpawnableData_RequiresLock;
};