// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Point/VoxelPointHandleProvider.h"
#include "VoxelInstancedCollisionComponent.generated.h"

struct FVoxelInvoker;
struct FVoxelMeshDataBase;

UCLASS()
class VOXELSPAWNER_API UVoxelInstancedCollisionComponent
	: public UPrimitiveComponent
	, public IVoxelPointHandleProvider
{
	GENERATED_BODY()

public:
	UVoxelInstancedCollisionComponent() = default;

	//~ Begin UPrimitiveComponent Interface
	virtual UBodySetup* GetBodySetup() override;
	virtual bool ShouldCreatePhysicsState() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	//~ End UPrimitiveComponent Interface

	//~ Begin IVoxelPointHandleProvider Interface
	virtual bool TryGetPointHandle(
		int32 ItemIndex,
		FVoxelPointHandle& OutHandle) const override;
	//~ End IVoxelPointHandleProvider Interface

	void SetStaticMesh(UStaticMesh* Mesh);
	void SetInvokerChannel(FName NewInvokerChannel);
	void SetMeshData(const TSharedRef<const FVoxelMeshDataBase>& NewMeshData);
	void UpdateInstances(TConstVoxelArrayView<int32> Indices);
	void RemoveInstances(TConstVoxelArrayView<int32> Indices);

	FORCEINLINE bool HasMeshData() const
	{
		return MeshData.IsValid();
	}

public:
	void CreateCollision();
	void ResetCollision();

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	TSharedPtr<const FVoxelMeshDataBase> MeshData;
	FName InvokerChannel;
	TVoxelArray<TVoxelUniquePtr<FBodyInstance>> InstanceBodies;

	bool bAllInstancesEnabled = false;
	TSharedPtr<const TVoxelAddOnlySet<int32>> EnabledInstances;

	void CreateAllBodies();
	TVoxelUniquePtr<FBodyInstance> MakeBodyInstance(int32 Index) const;

	friend class FVoxelInstancedCollisionSceneProxy;
	friend class FVoxelInstancedCollisionComponentManager;
};