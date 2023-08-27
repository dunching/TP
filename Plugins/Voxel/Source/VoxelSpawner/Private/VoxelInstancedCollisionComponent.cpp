// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelInstancedCollisionComponent.h"
#include "VoxelInstancedCollisionComponentManager.h"
#include "VoxelMeshDataBase.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "PrimitiveSceneProxy.h"
#include "PhysicsEngine/BodySetup.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELSPAWNER_API, bool, GVoxelFoliageShowInstancesCollisions, true,
	"voxel.foliage.ShowInstancesCollisions",
	"");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UBodySetup* UVoxelInstancedCollisionComponent::GetBodySetup()
{
	if (!StaticMesh)
	{
		return nullptr;
	}

	return StaticMesh->GetBodySetup();
}

bool UVoxelInstancedCollisionComponent::ShouldCreatePhysicsState() const
{
	return true;
}

FBoxSphereBounds UVoxelInstancedCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!StaticMesh ||
		!MeshData ||
		!ensure(MeshData->Num() > 0) ||
		!ensure(MeshData->Bounds.IsValid()))
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}

	return MeshData->Bounds->TransformBy(GetComponentToWorld()).ToFBox();
}

void UVoxelInstancedCollisionComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	VOXEL_FUNCTION_COUNTER();

	ResetCollision();

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UVoxelInstancedCollisionComponent::OnCreatePhysicsState()
{
	// We want to avoid PrimitiveComponent base body instance at component location
	USceneComponent::OnCreatePhysicsState();
}

void UVoxelInstancedCollisionComponent::OnDestroyPhysicsState()
{
#if UE_ENABLE_DEBUG_DRAWING
	SendRenderDebugPhysics();
#endif

	// We want to avoid PrimitiveComponent base body instance at component location
	USceneComponent::OnDestroyPhysicsState();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UVoxelInstancedCollisionComponent::TryGetPointHandle(
	const int32 ItemIndex,
	FVoxelPointHandle& OutHandle) const
{
	if (!ensure(MeshData))
	{
		return false;
	}

	if (!ensure(MeshData->PointIds.IsValidIndex(ItemIndex)))
	{
		return false;
	}

	OutHandle.SpawnableRef = MeshData->SpawnableRef;
	OutHandle.PointId = MeshData->PointIds[ItemIndex];
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelInstancedCollisionComponent::SetStaticMesh(UStaticMesh* Mesh)
{
	ensure(IsRegistered());
	StaticMesh = Mesh;

	if (const UBodySetup* BodySetup = GetBodySetup())
	{
		if (!BodyInstance.GetOverrideWalkableSlopeOnInstance())
		{
			BodyInstance.SetWalkableSlopeOverride(BodySetup->WalkableSlopeOverride, false);
		}
	}
}

void UVoxelInstancedCollisionComponent::SetInvokerChannel(const FName NewInvokerChannel)
{
	InvokerChannel = NewInvokerChannel;
	GVoxelInstancedCollisionComponentTicker->UpdateComponent(this);
}

void UVoxelInstancedCollisionComponent::SetMeshData(const TSharedRef<const FVoxelMeshDataBase>& NewMeshData)
{
	ensure(InstanceBodies.Num() == 0);
	ensure(NewMeshData->Num() > 0);
	MeshData = NewMeshData;

	bAllInstancesEnabled = false;
	EnabledInstances.Reset();
}

void UVoxelInstancedCollisionComponent::UpdateInstances(const TConstVoxelArrayView<int32> Indices)
{
	VOXEL_FUNCTION_COUNTER();

	// Not supported
	ensure(InvokerChannel.IsNone());

	UBodySetup* BodySetup = GetBodySetup();
	FPhysScene* PhysicsScene = GetWorld()->GetPhysicsScene();
	if (!ensure(StaticMesh) ||
		!ensure(MeshData) ||
		!ensure(BodySetup) ||
		!ensure(PhysicsScene))
	{
		return;
	}

	ensure(InstanceBodies.Num() <= MeshData->Num());
	InstanceBodies.SetNum(MeshData->Num());

	TCompatibleVoxelArray<FBodyInstance*> ValidBodyInstances;
	TCompatibleVoxelArray<FTransform> ValidTransforms;
	ValidBodyInstances.Reserve(MeshData->Num());
	ValidTransforms.Reserve(MeshData->Num());

	for (const int32 Index : Indices)
	{
		if (!ensure(MeshData->Transforms.IsValidIndex(Index) ||
			!ensure(InstanceBodies.IsValidIndex(Index))))
		{
			continue;
		}

		const FTransform InstanceTransform = FTransform(MeshData->Transforms[Index]) * GetComponentTransform();

		TVoxelUniquePtr<FBodyInstance>& Body = InstanceBodies[Index];
		if (Body)
		{
			Body->TermBody();
		}
		Body.Reset();

		if (InstanceTransform.GetScale3D().IsNearlyZero())
		{
			continue;
		}

		Body = MakeBodyInstance(Index);

		ValidBodyInstances.Add(Body.Get());
		ValidTransforms.Add(InstanceTransform);
	}
	check(ValidBodyInstances.Num() == ValidTransforms.Num());

	if (ValidBodyInstances.Num() == 0)
	{
		return;
	}

	FBodyInstance::InitStaticBodies(ValidBodyInstances, ValidTransforms, BodySetup, this, PhysicsScene);
	MarkRenderStateDirty();
}

void UVoxelInstancedCollisionComponent::RemoveInstances(const TConstVoxelArrayView<int32> Indices)
{
	VOXEL_FUNCTION_COUNTER();

	// Not supported
	ensure(InvokerChannel.IsNone());

	if (!ensure(MeshData))
	{
		return;
	}

	for (const int32 Index : Indices)
	{
		if (!ensure(InstanceBodies.IsValidIndex(Index)))
		{
			continue;
		}

		TVoxelUniquePtr<FBodyInstance>& Body = InstanceBodies[Index];
		if (!Body)
		{
			continue;
		}

		Body->TermBody();
		Body.Reset();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelInstancedCollisionComponent::CreateCollision()
{
	VOXEL_FUNCTION_COUNTER();

	ensure(InstanceBodies.Num() == 0);
	FVoxelUtilities::SetNum(InstanceBodies, MeshData->Num());

	if (InvokerChannel.IsNone())
	{
		CreateAllBodies();
	}
}

void UVoxelInstancedCollisionComponent::ResetCollision()
{
	VOXEL_SCOPE_COUNTER_FORMAT("UVoxelInstancedCollisionComponent::ResetCollision Num=%d", InstanceBodies.Num());

	for (TVoxelUniquePtr<FBodyInstance>& Body : InstanceBodies)
	{
		if (Body)
		{
			Body->TermBody();
		}
	}

	InvokerChannel = {};
	MeshData.Reset();
	InstanceBodies.Empty();

	bAllInstancesEnabled = false;
	EnabledInstances.Reset();

	MarkRenderStateDirty();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelInstancedCollisionSceneProxy : public FPrimitiveSceneProxy
{
	const UVoxelInstancedCollisionComponent& Component;
	const TSharedRef<const FVoxelMeshDataBase> MeshData;
	const UBodySetup* BodySetup;

public:
	explicit FVoxelInstancedCollisionSceneProxy(UVoxelInstancedCollisionComponent& Component)
		: FPrimitiveSceneProxy(&Component)
		, Component(Component)
		, MeshData(Component.MeshData.ToSharedRef())
		, BodySetup(Component.GetBodySetup())
	{
	}

	//~ Begin FPrimitiveSceneProxy Interface
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		const uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		VOXEL_FUNCTION_COUNTER_LLM();

		if (!GVoxelFoliageShowInstancesCollisions)
		{
			return;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}

			ensure(
				Views[ViewIndex]->Family->EngineShowFlags.Collision ||
				Views[ViewIndex]->Family->EngineShowFlags.CollisionPawn ||
				Views[ViewIndex]->Family->EngineShowFlags.CollisionVisibility);

			const FMaterialRenderProxy* MaterialProxy = FVoxelRenderUtilities::CreateColoredRenderProxy(
				Collector,
				FColor(157, 149, 223, 255));

			const FTransform LocalToWorldTransform(GetLocalToWorld());
			for (int32 Index = 0; Index < MeshData->Num(); Index++)
			{
				if (!ensure(Component.InstanceBodies.IsValidIndex(Index)) ||
					!Component.InstanceBodies[Index].IsValid())
				{
					continue;
				}

				BodySetup->AggGeom.GetAggGeom(
					FTransform(MeshData->Transforms[Index]) * LocalToWorldTransform,
					FColor(157, 149, 223, 255),
					MaterialProxy,
					false,
					true,
					DrawsVelocity(),
					ViewIndex,
					Collector);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = true;
		Result.bRenderInMainPass = true;
		Result.bDynamicRelevance =
			View->Family->EngineShowFlags.Collision ||
			View->Family->EngineShowFlags.CollisionPawn ||
			View->Family->EngineShowFlags.CollisionVisibility;
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}
	//~ End FPrimitiveSceneProxy Interface
};

FPrimitiveSceneProxy* UVoxelInstancedCollisionComponent::CreateSceneProxy()
{
	if (!GIsEditor ||
		!GVoxelFoliageShowInstancesCollisions ||
		!MeshData)
	{
		return nullptr;
	}

	return new FVoxelInstancedCollisionSceneProxy(*this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelInstancedCollisionComponent::CreateAllBodies()
{
	VOXEL_FUNCTION_COUNTER();

	if (bAllInstancesEnabled)
	{
		return;
	}
	bAllInstancesEnabled = true;

	UBodySetup* BodySetup = GetBodySetup();
	FPhysScene* PhysicsScene = GetWorld()->GetPhysicsScene();
	if (!ensure(MeshData) ||
		!ensure(BodySetup) ||
		!ensure(PhysicsScene))
	{
		return;
	}

	VOXEL_SCOPE_COUNTER_FORMAT("CreateAllBodies Num=%d", MeshData->Num());

	TCompatibleVoxelArray<FBodyInstance*> ValidBodyInstances;
	TCompatibleVoxelArray<FTransform> ValidTransforms;
	ValidBodyInstances.Reserve(MeshData->Num());
	ValidTransforms.Reserve(MeshData->Num());

	for (int32 Index = 0; Index < MeshData->Num(); Index++)
	{
		TVoxelUniquePtr<FBodyInstance>& Body = InstanceBodies[Index];

		const FTransform InstanceTransform = FTransform(MeshData->Transforms[Index]) * GetComponentTransform();
		if (InstanceTransform.GetScale3D().IsNearlyZero())
		{
			if (Body)
			{
				Body->TermBody();
			}
			Body.Reset();
			continue;
		}

		if (Body)
		{
			ensure(Body->InstanceBodyIndex == Index);
			continue;
		}

		Body = MakeBodyInstance(Index);

		ValidBodyInstances.Add(Body.Get());
		ValidTransforms.Add(InstanceTransform);

		InstanceBodies[Index] = MoveTemp(Body);
	}
	check(ValidBodyInstances.Num() == ValidTransforms.Num());

	if (ValidBodyInstances.Num() == 0)
	{
		return;
	}

	FBodyInstance::InitStaticBodies(ValidBodyInstances, ValidTransforms, BodySetup, this, PhysicsScene);
	MarkRenderStateDirty();
}

TVoxelUniquePtr<FBodyInstance> UVoxelInstancedCollisionComponent::MakeBodyInstance(const int32 Index) const
{
	TVoxelUniquePtr<FBodyInstance> Body = MakeVoxelUnique<FBodyInstance>();
	FVoxelGameUtilities::CopyBodyInstance(*Body, BodyInstance);
	Body->bAutoWeld = false;
	Body->bSimulatePhysics = false;
	Body->InstanceBodyIndex = Index;
	return Body;
}