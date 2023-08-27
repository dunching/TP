// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelMeshDataBase.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "VoxelHierarchicalMeshComponent.generated.h"

struct FVoxelInvoker;
class UVoxelInstancedCollisionComponent;

DECLARE_VOXEL_MEMORY_STAT(VOXELSPAWNER_API, STAT_VoxelHierarchicalMeshDataMemory, "Voxel Hierarchical Mesh Data Memory");
DECLARE_VOXEL_COUNTER(VOXELSPAWNER_API, STAT_VoxelHierarchicalMeshNumInstances, "Num Hierarchical Mesh Instances");

struct VOXELSPAWNER_API FVoxelHierarchicalMeshBuiltData
{
	TUniquePtr<FStaticMeshInstanceData> InstanceBuffer;
	TArray<FClusterNode> ClusterTree;
	int32 OcclusionLayerNum = 0;

	TCompatibleVoxelArray<FInstancedStaticMeshInstanceData> InstanceDatas;
	TCompatibleVoxelArray<float> CustomDatas;
	TCompatibleVoxelArray<int32> InstanceReorderTable;
};

struct VOXELSPAWNER_API FVoxelHierarchicalMeshData : public FVoxelMeshDataBase
{
	using FVoxelMeshDataBase::FVoxelMeshDataBase;

	mutable TSharedPtr<FVoxelHierarchicalMeshBuiltData> BuiltData;

	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelHierarchicalMeshDataMemory);

	void Build();
};

UCLASS()
class VOXELSPAWNER_API UVoxelHierarchicalMeshComponent : public UHierarchicalInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UVoxelInstancedCollisionComponent> CollisionComponent;

	UVoxelHierarchicalMeshComponent();

	TSharedPtr<const FVoxelHierarchicalMeshData> GetMeshData() const
	{
		return MeshData;
	}

	//~ Begin UPrimitiveComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	//~ End UPrimitiveComponent Interface

	void SetMeshData(const TSharedRef<const FVoxelHierarchicalMeshData>& NewMeshData);
	void UpdateCollision(
		FName InvokerChannel,
		const FBodyInstance& NewBodyInstance) const;
	void RemoveInstancesFast(TConstVoxelArrayView<int32> Indices);
	void ReturnToPool();

	void ReleasePerInstanceRenderData_Safe();

	virtual void ClearInstances() override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

	static void AsyncTreeBuild(
		FVoxelHierarchicalMeshBuiltData& OutBuiltData,
		const FBox& MeshBox,
		int32 InDesiredInstancesPerLeaf,
		const FVoxelHierarchicalMeshData& InMeshData);

private:
	VOXEL_COUNTER_HELPER(STAT_VoxelHierarchicalMeshNumInstances, NumInstances);

	TSharedPtr<const FVoxelHierarchicalMeshData> MeshData;

	void SetBuiltData(FVoxelHierarchicalMeshBuiltData&& BuiltData);
};