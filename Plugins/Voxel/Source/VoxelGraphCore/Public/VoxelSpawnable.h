// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelQuery.h"
#include "VoxelNodeHelpers.h"
#include "VoxelSpawnableRef.h"
#include "VoxelSpawnable.generated.h"

class FVoxelRuntime;
struct FVoxelPointId;

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelSpawnable
	: public FVoxelVirtualStruct
	, public IVoxelNodeInterface
	, public TSharedFromThis<FVoxelSpawnable>
	, public TVoxelRuntimeInfo<FVoxelSpawnable>
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	virtual ~FVoxelSpawnable() override;

	FORCEINLINE bool IsValid() const
	{
		return GetStruct() != StaticStruct();
	}
	FORCEINLINE bool IsCreated() const
	{
		ensureVoxelSlow(IsInGameThreadFast());
		return bIsCreated;
	}
	FORCEINLINE bool IsDestroyed() const
	{
		ensureVoxelSlow(IsInGameThreadFast());
		return bIsDestroyed;
	}

	FORCEINLINE const FVoxelSpawnableRef& GetSpawnableRef() const
	{
		return PrivateSpawnableRef;
	}
	FORCEINLINE const FVoxelRuntimeInfo& GetRuntimeInfoRef() const
	{
		return *RuntimeInfo;
	}

	//~ Begin IVoxelNodeInterface Interface
	FORCEINLINE virtual const FVoxelGraphNodeRef& GetNodeRef() const final override
	{
		return PrivateNode;
	}
	//~ End IVoxelNodeInterface Interface

	void CallCreate_AnyThread();
	void CallDestroy_AnyThread();

	virtual FVoxelOptionalBox GetBounds() const { return {}; }

	virtual bool GetPointAttributes(
		TConstVoxelArrayView<FVoxelPointId> PointIds,
		TVoxelMap<FName, TSharedPtr<const FVoxelBuffer>>& OutAttributes) const VOXEL_PURE_VIRTUAL({});

protected:
	virtual void Create_AnyThread() {}
	virtual void Destroy_AnyThread() {}

private:
	bool bIsCreated = false;
	bool bIsDestroyed = false;
	FVoxelGraphNodeRef PrivateNode;
	FVoxelSpawnableRef PrivateSpawnableRef;
	TSharedPtr<const FVoxelRuntimeInfo> RuntimeInfo;

	void InitializeInternal(
		const FVoxelQuery& Query,
		const FVoxelGraphNodeRef& Node);

	template<typename SpawnableType>
	friend TSharedRef<SpawnableType> MakeVoxelSpawnable(
		const FVoxelQuery& Query,
		const FVoxelGraphNodeRef& Node);
};

template<typename SpawnableType>
TSharedRef<SpawnableType> MakeVoxelSpawnable(
	const FVoxelQuery& Query,
	const FVoxelGraphNodeRef& Node)
{
	const TSharedRef<SpawnableType> Result = MakeVoxelShared<SpawnableType>();
	Result->InitializeInternal(Query, Node);
	return Result;
}

VOXELGRAPHCORE_API TSharedRef<const FVoxelQueryParameters> MakeVoxelSpawnablePointQueryParameters(
	const TSharedRef<const FVoxelQueryContext>& QueryContext,
	const TSharedRef<const FVoxelQueryParameters>& QueryParameters,
	const FVoxelGraphNodeRef& Node);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelMergeSpawnableRefQueryParameter : public FVoxelQueryParameter
{
	GENERATED_BODY()
	GENERATED_VOXEL_QUERY_PARAMETER_BODY()

	FVoxelMergeSpawnableRef MergeSpawnableRef;
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelSpawnableRefQueryParameter : public FVoxelQueryParameter
{
	GENERATED_BODY()
	GENERATED_VOXEL_QUERY_PARAMETER_BODY()

	FVoxelSpawnableRef SpawnableRef;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelGameThreadSpawnable : public FVoxelSpawnable
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	//~ Begin FVoxelSpawnable Interface
	virtual void Create_AnyThread() final override;
	virtual void Destroy_AnyThread() final override;
	//~ End FVoxelSpawnable Interface

	virtual void Create_GameThread() {}
	virtual void Destroy_GameThread() {}
};