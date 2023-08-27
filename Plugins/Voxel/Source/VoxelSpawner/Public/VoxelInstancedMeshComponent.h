﻿// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelMeshDataBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "VoxelInstancedMeshComponent.generated.h"

class UVoxelInstancedCollisionComponent;

DECLARE_VOXEL_MEMORY_STAT(VOXELSPAWNER_API, STAT_VoxelInstancedMeshDataMemory, "Voxel Instanced Mesh Data Memory");
DECLARE_VOXEL_COUNTER(VOXELSPAWNER_API, STAT_VoxelInstancedMeshNumInstances, "Num Instanced Mesh Instances");

struct VOXELSPAWNER_API FVoxelInstancedMeshData : public FVoxelMeshDataBase
{
	using FVoxelMeshDataBase::FVoxelMeshDataBase;

	int32 NumNewInstances = 0;
	TVoxelArray<int32> InstanceIndices;

	TCompatibleVoxelArray<FInstancedStaticMeshInstanceData> PerInstanceSMData;
	TCompatibleVoxelArray<float> PerInstanceSMCustomData;

	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelInstancedMeshDataMemory);

	void Build();
	int64 GetAllocatedSize() const;
};

UCLASS()
class VOXELSPAWNER_API UVoxelInstancedMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UVoxelInstancedCollisionComponent> CollisionComponent;

	UVoxelInstancedMeshComponent();

	//~ Begin UPrimitiveComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	//~ End UPrimitiveComponent Interface

	void AddMeshData(const TSharedRef<FVoxelInstancedMeshData>& NewMeshData);
	void SetMeshData(const TSharedRef<FVoxelInstancedMeshData>& NewMeshData);
	void UpdateCollision(const FBodyInstance& NewBodyInstance) const;
	void RemoveInstancesFast(TConstVoxelArrayView<int32> Indices);
	void ReturnToPool();

	virtual void ClearInstances() override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

	FORCEINLINE bool HasMeshData() const
	{
		return MeshData.IsValid();
	}

private:
	VOXEL_COUNTER_HELPER(STAT_VoxelInstancedMeshNumInstances, NumInstances);

	TSharedPtr<FVoxelInstancedMeshData> MeshData;
};