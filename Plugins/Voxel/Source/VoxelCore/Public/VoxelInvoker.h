// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelAABBTree.h"
#include "VoxelInvoker.generated.h"

class FVoxelInvokerManager;

DECLARE_VOXEL_SPARSE_INDEX(FVoxelInvokerId);

class VOXELCORE_API FVoxelInvokerRef
{
public:
	~FVoxelInvokerRef();

private:
	FVoxelInvokerId InvokerId;

	explicit FVoxelInvokerRef(const FVoxelInvokerId InvokerId)
		: InvokerId(InvokerId)
	{
	}

	friend FVoxelInvokerManager;
};

struct VOXELCORE_API FVoxelInvoker
{
	const FVoxelBox Bounds;
	const TWeakObjectPtr<const UObject> DebugObject;

	explicit FVoxelInvoker(
		const FVoxelBox& Bounds,
		const TWeakObjectPtr<const UObject>& DebugObject)
		: Bounds(Bounds)
		, DebugObject(DebugObject)
	{
	}
	virtual ~FVoxelInvoker() = default;

	virtual bool Intersect(const FVoxelBox& InBounds) const = 0;
};

struct VOXELCORE_API FVoxelDummyInvoker : FVoxelInvoker
{
	FVoxelDummyInvoker()
		: FVoxelInvoker(FVoxelBox(), nullptr)
	{
	}

	virtual bool Intersect(const FVoxelBox& InBounds) const override
	{
		return false;
	}
};

struct VOXELCORE_API FVoxelInvokerUnion : FVoxelInvoker
{
public:
	const TVoxelArray<TSharedRef<const FVoxelInvoker>> Invokers;

	explicit FVoxelInvokerUnion(const TVoxelArray<TSharedRef<const FVoxelInvoker>>& Invokers);

	virtual bool Intersect(const FVoxelBox& InBounds) const override;

private:
	FVoxelAABBTree Tree;
};

extern VOXELCORE_API FVoxelInvokerManager* GVoxelInvokerManager;

class VOXELCORE_API FVoxelInvokerManager
{
public:
	TSharedRef<const FVoxelInvoker> GetInvoker(FObjectKey World, FName Channel) const;
	TSharedRef<FVoxelInvokerRef> AddInvoker(
		UWorld& World,
		const TSet<FName>& Channels,
		const TSharedRef<const FVoxelInvoker>& Invoker);

	void LogAllInvokers();

private:
	mutable FVoxelFastCriticalSection CriticalSection;
	TVoxelMap<FObjectKey, TVoxelMap<FName, TSharedPtr<const FVoxelInvoker>>> WorldToChannelToInvoker_RequiresLock;

	struct FInvoker
	{
		FObjectKey World;
		TSet<FName> Channels;
		TSharedPtr<const FVoxelInvoker> Invoker;
	};
	bool bRebuildQueued = false;
	TVoxelSparseArray<FInvoker, FVoxelInvokerId> Invokers_GameThread;

	void RebuildChannelInvokers_GameThread();

	friend class FVoxelInvokerRef;
	friend class FVoxelInvokerTicker;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS(Abstract, HideCategories = ("Rendering", "Physics", "LOD", "Activation", "Collision", "Cooking", "AssetUserData"), meta = (BlueprintSpawnableComponent))
class VOXELCORE_API UVoxelInvokerComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invoker")
	TSet<FName> Channels = { "Default" };

	// If the component was moved less than this value, it won't update
	// Useful to avoid triggering too many invoker updates
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invoker", AdvancedDisplay)
	double TranslationTolerance = 1.;

	// If the component was rotated less than this value, it won't update
	// Useful to avoid triggering too many invoker updates
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invoker", AdvancedDisplay)
	double RotationTolerance = 0.001;

	// If the component was scaled less than this value, it won't update
	// Useful to avoid triggering too many invoker updates
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invoker", AdvancedDisplay)
	double ScaleTolerance = 0.;

	UVoxelInvokerComponent();
	virtual ~UVoxelInvokerComponent() override;

	//~ Begin USceneComponent Interface
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	//~ End USceneComponent Interface

	virtual TSharedPtr<FVoxelInvoker> CreateInvoker() const VOXEL_PURE_VIRTUAL({});

	UFUNCTION(BlueprintCallable, Category = "Voxel|Invoker")
	void UpdateInvoker();

private:
	TSharedPtr<FVoxelInvokerRef> InvokerRef;
	TOptional<FTransform> LastTransform;

	friend class FVoxelInvokerTicker;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct VOXELCORE_API FVoxelSphereInvoker : FVoxelInvoker
{
	const FMatrix WorldToLocal;
	const double Radius;
	const FVector Center;

	FVoxelSphereInvoker(
		const FMatrix& WorldToLocal,
		const double Radius,
		const TWeakObjectPtr<const UObject>& DebugObject)
		: FVoxelInvoker(FVoxelBox(-Radius, Radius).TransformBy(WorldToLocal.Inverse()), DebugObject)
		, WorldToLocal(WorldToLocal)
		, Radius(Radius)
		, Center(WorldToLocal.InverseTransformPosition(FVector::ZeroVector))
	{
	}

	virtual bool Intersect(const FVoxelBox& InBounds) const override
	{
		return InBounds.ComputeSquaredDistanceFromBoxToPoint(Center) < FMath::Square(Radius);
	}
};

UCLASS(ClassGroup = Voxel, meta = (BlueprintSpawnableComponent))
class VOXELCORE_API UVoxelSphereInvokerComponent : public UVoxelInvokerComponent
{
	GENERATED_BODY()

public:
	// In world space, not affected by scale
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invoker")
	double Radius = 1000.;

	//~ Begin UVoxelInvokerComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual TSharedPtr<FVoxelInvoker> CreateInvoker() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End UVoxelInvokerComponent Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct VOXELCORE_API FVoxelBoxInvoker : FVoxelInvoker
{
	const FMatrix WorldToLocal;
	const FVoxelBox Box;

	FVoxelBoxInvoker(
		const FMatrix& WorldToLocal,
		const FVoxelBox& Box,
		const TWeakObjectPtr<const UObject>& DebugObject)
		: FVoxelInvoker(Box.TransformBy(WorldToLocal.Inverse()), DebugObject)
		, WorldToLocal(WorldToLocal)
		, Box(Box)
	{
	}

	virtual bool Intersect(const FVoxelBox& InBounds) const override
	{
		return InBounds.TransformBy(WorldToLocal).Intersect(Box);
	}
};

UCLASS(ClassGroup = Voxel, meta = (BlueprintSpawnableComponent))
class VOXELCORE_API UVoxelBoxInvokerComponent : public UVoxelInvokerComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invoker")
	FVector Extent = FVector(1000.);

	//~ Begin UVoxelInvokerComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual TSharedPtr<FVoxelInvoker> CreateInvoker() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End UVoxelInvokerComponent Interface
};