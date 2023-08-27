// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelSpawnable.h"
#include "Point/VoxelPointSet.h"
#include "VoxelFoliageSettings.h"
#include "Buffer/VoxelStaticMeshBuffer.h"
#include "VoxelSpawnMeshNode.generated.h"

class UVoxelInstancedMeshComponent;
class UVoxelHierarchicalMeshComponent;
struct FVoxelInvoker;
struct FVoxelHierarchicalMeshData;

USTRUCT(Category = "Point")
struct VOXELSPAWNER_API FVoxelNode_SpawnMesh : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelPointSet, In, nullptr, VirtualPin);
	VOXEL_INPUT_PIN(FBodyInstance, BodyInstance, ECollisionEnabled::NoCollision, VirtualPin);
	VOXEL_INPUT_PIN(FVoxelFoliageSettings, FoliageSettings, nullptr, VirtualPin);
	// Distance from camera at which each instance begins to fade out
	VOXEL_INPUT_PIN(float, StartCullDistance, 1000000, VirtualPin);
	// Distance from camera at which each instance completely fades out
	VOXEL_INPUT_PIN(float, EndCullDistance, 2000000, VirtualPin);
	// If set, only instances overlapping this invoker will have collision added to the Chaos scene
	VOXEL_INPUT_PIN(FName, CollisionInvokerChannel, nullptr, VirtualPin, AdvancedDisplay);
	VOXEL_OUTPUT_PIN(FVoxelSpawnable, Out, VirtualPin);
};

USTRUCT()
struct VOXELSPAWNER_API FVoxelSpawnMeshSpawnable : public FVoxelGameThreadSpawnable
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	void Initialize(
		const FVoxelQuery& Query,
		const FVoxelNode_SpawnMesh& SpawnMesh);

	//~ Begin FVoxelGameThreadExecutedValue Interface
	virtual void Create_GameThread() override;
	virtual void Destroy_GameThread() override;
	virtual FVoxelOptionalBox GetBounds() const override;

	virtual bool GetPointAttributes(
		TConstVoxelArrayView<FVoxelPointId> PointIds,
		TVoxelMap<FName, TSharedPtr<const FVoxelBuffer>>& OutAttributes) const override;
	//~ End FVoxelGameThreadExecutedValue Interface

private:
	TSharedPtr<const FBodyInstance> BodyInstance_GameThread;
	TSharedPtr<const FVoxelFoliageSettings> FoliageSettings_GameThread;
	float StartCullDistance_GameThread = 0.f;
	float EndCullDistance_GameThread = 0.f;
	TOptional<FName> CollisionInvokerChannel_GameThread;

	mutable FVoxelFastCriticalSection CriticalSection;
	TVoxelAddOnlyMap<FVoxelPointId, int32> PointIdToIndex_RequiresLock;
	TSharedPtr<const FVoxelPointSet> Points_RequiresLock;

	struct FIndexInfo
	{
		union
		{
			struct
			{
				uint32 Index : 30;
				uint32 bIsValid : 1;
				uint32 bIsHierarchical : 1;
			};
			uint32 Raw = 0;
		};
	};
	checkStatic(sizeof(FIndexInfo) == sizeof(uint32));
	struct FComponents
	{
		TVoxelAddOnlyMap<FVoxelPointId, FIndexInfo> PointIdToIndexInfo;
		TVoxelArray<int32> FreeInstancedIndices;
		int32 NumInstancedInstances = 0;

		TWeakObjectPtr<UVoxelInstancedMeshComponent> InstancedMeshComponent;
		TWeakObjectPtr<UVoxelHierarchicalMeshComponent> HierarchicalMeshComponent;
	};
	TVoxelMap<FVoxelStaticMesh, TSharedPtr<FComponents>> MeshToComponents_RequiresLock;

	TVoxelArray<TSharedPtr<FVoxelHierarchicalMeshData>> PendingHierarchicalMeshDatas_GameThread;

	void UpdatePoints(const TSharedRef<const FVoxelPointSet>& NewPoints);
	void UpdatePoints_Hierarchical_AssumeLocked(const TSharedRef<const FVoxelPointSet>& Points);
	void SetHierarchicalDatas_GameThread(const TVoxelArray<TSharedPtr<FVoxelHierarchicalMeshData>>& NewHierarchicalMeshDatas);
	void FlushPendingHierarchicalDatas_GameThread();

private:
	TVoxelDynamicValue<FVoxelPointSet> PointsValue;
	TVoxelDynamicValue<FBodyInstance> BodyInstanceValue;
	TVoxelDynamicValue<FVoxelFoliageSettings> FoliageSettingsValue;
	TVoxelDynamicValue<float> StartCullDistanceValue;
	TVoxelDynamicValue<float> EndCullDistanceValue;
	TVoxelDynamicValue<FName> CollisionInvokerChannelValue;
};