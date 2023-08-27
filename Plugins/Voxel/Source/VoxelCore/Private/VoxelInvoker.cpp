// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelInvoker.h"
#include "SceneManagement.h"
#include "PrimitiveSceneProxy.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelShowInvokers, false,
	"voxel.ShowInvokers",
	"");

VOXEL_CONSOLE_COMMAND(
	LogAllInvokers,
	"voxel.LogAllInvokers",
	"")
{
	GVoxelInvokerManager->LogAllInvokers();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelInvokerManager* GVoxelInvokerManager;

VOXEL_RUN_ON_STARTUP_GAME(CreateVoxelInvokerManager)
{
	GVoxelInvokerManager = new FVoxelInvokerManager();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelInvokerRef::~FVoxelInvokerRef()
{
	checkVoxelSlow(IsInGameThread());
	GVoxelInvokerManager->Invokers_GameThread.RemoveAt(InvokerId);
	GVoxelInvokerManager->bRebuildQueued = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelInvokerUnion::FVoxelInvokerUnion(const TVoxelArray<TSharedRef<const FVoxelInvoker>>& Invokers)
	: FVoxelInvoker(INLINE_LAMBDA
	{
		ensure(Invokers.Num() > 1);

		FVoxelBox Result = FVoxelBox::InvertedInfinite;
		for (const TSharedRef<const FVoxelInvoker>& Invoker : Invokers)
		{
			Result += Invoker->Bounds;
		}
		return Result;
	}, nullptr)
	, Invokers(Invokers)
{
	TVoxelArray<FVoxelAABBTree::FElement> Elements;
	Elements.Reserve(Invokers.Num());

	for (int32 Index = 0; Index < Invokers.Num(); Index++)
	{
		Elements.Add(FVoxelAABBTree::FElement
		{
			Invokers[Index]->Bounds,
			Index
		});
	}

	Tree.Initialize(MoveTemp(Elements));
}

bool FVoxelInvokerUnion::Intersect(const FVoxelBox& InBounds) const
{
	return Tree.Overlap(Bounds, [&](const int32 Index)
	{
		return Invokers[Index]->Intersect(InBounds);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<const FVoxelInvoker> FVoxelInvokerManager::GetInvoker(
	const FObjectKey World,
	const FName Channel) const
{
	VOXEL_SCOPE_LOCK(CriticalSection);

	static const TSharedRef<FVoxelDummyInvoker> DummyInvoker = MakeVoxelShared<FVoxelDummyInvoker>();

	const TVoxelMap<FName, TSharedPtr<const FVoxelInvoker>>* ChannelToInvokerPtr = WorldToChannelToInvoker_RequiresLock.Find(World);
	if (!ChannelToInvokerPtr)
	{
		return DummyInvoker;
	}

	const TSharedPtr<const FVoxelInvoker> Invoker = ChannelToInvokerPtr->FindRef(Channel);
	if (!Invoker)
	{
		return DummyInvoker;
	}
	return Invoker.ToSharedRef();
}

TSharedRef<FVoxelInvokerRef> FVoxelInvokerManager::AddInvoker(
	UWorld& World,
	const TSet<FName>& Channels,
	const TSharedRef<const FVoxelInvoker>& Invoker)
{
	checkVoxelSlow(IsInGameThread());

	const FVoxelInvokerId InvokerId = Invokers_GameThread.Add({ &World, Channels, Invoker });
	bRebuildQueued = true;
	return MakeVoxelShareable(new (GVoxelMemory) FVoxelInvokerRef(InvokerId));
}

void FVoxelInvokerManager::LogAllInvokers()
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(IsInGameThread());

	TVoxelAddOnlyMap<FObjectKey, TVoxelAddOnlyMap<FName, TVoxelArray<TSharedRef<const FVoxelInvoker>>>> WorldToChannelToInvokers;
	for (const FInvoker& Invoker : Invokers_GameThread)
	{
		TVoxelAddOnlyMap<FName, TVoxelArray<TSharedRef<const FVoxelInvoker>>>& ChannelToInvokers =WorldToChannelToInvokers.FindOrAdd(Invoker.World);
		for (const FName Channel : Invoker.Channels)
		{
			ChannelToInvokers.FindOrAdd(Channel).Add(Invoker.Invoker.ToSharedRef());
		}
	}

	for (const auto& WorldIt : WorldToChannelToInvokers)
	{
		LOG_VOXEL(Log, "%s:", ensure(WorldIt.Key.ResolveObjectPtr()) ? *WorldIt.Key.ResolveObjectPtr()->GetPathName() : TEXT("null"));

		for (const auto& It : WorldIt.Value)
		{
			LOG_VOXEL(Log, "\tChannel '%s':", *It.Key.ToString());

			for (const TSharedRef<const FVoxelInvoker>& Invoker : It.Value)
			{
				if (Invoker->DebugObject.IsExplicitlyNull())
				{
					LOG_VOXEL(Log, "\t\tDummy");
					continue;
				}

				const UObject* DebugObject = Invoker->DebugObject.Get();
				if (!ensure(DebugObject))
				{
					LOG_VOXEL(Log, "\t\tnullptr");
					continue;
				}

				if (const UVoxelSphereInvokerComponent* SphereInvoker = Cast<UVoxelSphereInvokerComponent>(DebugObject))
				{
					LOG_VOXEL(Log, "\t\tSphere Invoker Radius=%f %s", SphereInvoker->Radius, *SphereInvoker->GetPathName());
				}
				else
				{
					LOG_VOXEL(Log, "\t\t%s", *DebugObject->GetPathName());
				}
			}
		}
	}
}

void FVoxelInvokerManager::RebuildChannelInvokers_GameThread()
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(IsInGameThread());

	TVoxelAddOnlyMap<FObjectKey, TVoxelAddOnlyMap<FName, TVoxelArray<TSharedRef<const FVoxelInvoker>>>> WorldToChannelToInvokers;
	for (const FInvoker& Invoker : Invokers_GameThread)
	{
		TVoxelAddOnlyMap<FName, TVoxelArray<TSharedRef<const FVoxelInvoker>>>& ChannelToInvokers =WorldToChannelToInvokers.FindOrAdd(Invoker.World);
		for (const FName Channel : Invoker.Channels)
		{
			ChannelToInvokers.FindOrAdd(Channel).Add(Invoker.Invoker.ToSharedRef());
		}
	}

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, WorldToChannelToInvokers = MoveTemp(WorldToChannelToInvokers)]
	{
		VOXEL_SCOPE_COUNTER("Update ChannelToInvoker_RequiresLock")
		VOXEL_SCOPE_LOCK(CriticalSection);

		WorldToChannelToInvoker_RequiresLock.Reset();

		for (const auto& WorldIt : WorldToChannelToInvokers)
		{
			TVoxelMap<FName, TSharedPtr<const FVoxelInvoker>>& ChannelToInvokers = WorldToChannelToInvoker_RequiresLock.Add(WorldIt.Key);

			for (const auto& It : WorldIt.Value)
			{
				TSharedPtr<const FVoxelInvoker>& Invoker = ChannelToInvokers.Add(It.Key);
				const TVoxelArray<TSharedRef<const FVoxelInvoker>>& Invokers = It.Value;

				if (Invokers.Num() == 0)
				{
					Invoker = nullptr;
				}
				else if (Invokers.Num() == 1)
				{
					Invoker = Invokers[0];
				}
				else
				{
					Invoker = MakeVoxelShared<FVoxelInvokerUnion>(Invokers);
				}
			}
		}
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelInvokerTicker : public FVoxelTicker
{
public:
	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		ForEachObjectOfClass<UVoxelInvokerComponent>([&](UVoxelInvokerComponent* Component)
		{
			const FTransform Transform = Component->GetComponentTransform();

			if (!Component->LastTransform.IsSet() ||
				!Component->LastTransform->TranslationEquals(Transform, Component->TranslationTolerance) ||
				!Component->LastTransform->RotationEquals(Transform, Component->RotationTolerance) ||
				!Component->LastTransform->Scale3DEquals(Transform, Component->ScaleTolerance))
			{
				Component->LastTransform = Transform;
				Component->UpdateInvoker();
			}
		});

		if (GVoxelInvokerManager->bRebuildQueued)
		{
			GVoxelInvokerManager->RebuildChannelInvokers_GameThread();
		}
	}
	//~ End FVoxelTicker Interface
};

VOXEL_RUN_ON_STARTUP_GAME(MakeVoxelInvokerTicker)
{
	new FVoxelInvokerTicker();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelInvokerComponent::UVoxelInvokerComponent()
{
	bAutoActivate = true;
}

UVoxelInvokerComponent::~UVoxelInvokerComponent()
{
	ensure(!InvokerRef);
}

void UVoxelInvokerComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	UpdateInvoker();
}

void UVoxelInvokerComponent::Deactivate()
{
	Super::Deactivate();

	UpdateInvoker();
}

void UVoxelInvokerComponent::OnRegister()
{
	Super::OnRegister();

	UpdateInvoker();
}

void UVoxelInvokerComponent::OnUnregister()
{
	Super::OnUnregister();

	UpdateInvoker();
}

void UVoxelInvokerComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	UpdateInvoker();
}

void UVoxelInvokerComponent::UpdateInvoker()
{
	VOXEL_FUNCTION_COUNTER_LLM();

	InvokerRef = {};

	if (!IsRegistered() ||
		!IsActive())
	{
		return;
	}

	const TSharedPtr<FVoxelInvoker> Invoker = CreateInvoker();
	if (!Invoker)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!ensure(World))
	{
		return;
	}

	InvokerRef = GVoxelInvokerManager->AddInvoker(*World, Channels, Invoker.ToSharedRef());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FPrimitiveSceneProxy* UVoxelSphereInvokerComponent::CreateSceneProxy()
{
	class FSphereSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		const float Radius;

		explicit FSphereSceneProxy(const UVoxelSphereInvokerComponent* Component)
			: FPrimitiveSceneProxy(Component)
			, Radius(Component->Radius)
		{
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			VOXEL_FUNCTION_COUNTER();

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (!(VisibilityMap & (1 << ViewIndex)))
				{
					continue;
				}

				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				const FSceneView* View = Views[ViewIndex];
				const FMatrix LocalToWorld = GetLocalToWorld();

				const FLinearColor Color = GetViewSelectionColor(
					FColor::White,
					*View,
					IsSelected(),
					IsHovered(),
					false,
					IsIndividuallySelected());

				const FVector ScaledX = LocalToWorld.GetScaledAxis(EAxis::X);
				const FVector ScaledY = LocalToWorld.GetScaledAxis(EAxis::Y);
				const FVector ScaledZ = LocalToWorld.GetScaledAxis(EAxis::Z);

				DrawCircle(PDI, LocalToWorld.GetOrigin(), ScaledX, ScaledY, Color, Radius, 64, SDPG_World);
				DrawCircle(PDI, LocalToWorld.GetOrigin(), ScaledX, ScaledZ, Color, Radius, 64, SDPG_World);
				DrawCircle(PDI, LocalToWorld.GetOrigin(), ScaledY, ScaledZ, Color, Radius, 64, SDPG_World);
			}
		}
		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = (IsShown(View) && IsSelected()) || GVoxelShowInvokers;
			Result.bDynamicRelevance = true;
			return Result;
		}
		virtual SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}
		virtual uint32 GetMemoryFootprint() const override
		{
			return sizeof(*this) + GetAllocatedSize();
		}
	};
	return new FSphereSceneProxy(this);
}

TSharedPtr<FVoxelInvoker> UVoxelSphereInvokerComponent::CreateInvoker() const
{
	return MakeVoxelShared<FVoxelSphereInvoker>(
		GetComponentTransform().ToInverseMatrixWithScale(),
		Radius,
		this);
}

FBoxSphereBounds UVoxelSphereInvokerComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FVector::ZeroVector, FVector(Radius), Radius).TransformBy(LocalToWorld);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FPrimitiveSceneProxy* UVoxelBoxInvokerComponent::CreateSceneProxy()
{
	class FBoxSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		const FVector Extent;

		explicit FBoxSceneProxy(const UVoxelBoxInvokerComponent* Component)
			:	FPrimitiveSceneProxy(Component)
			,   Extent( Component->Extent )
		{
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			VOXEL_FUNCTION_COUNTER();

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (!(VisibilityMap & (1 << ViewIndex)))
				{
					continue;
				}

				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				const FSceneView* View = Views[ViewIndex];
				const FMatrix LocalToWorld = GetLocalToWorld();

				const FLinearColor Color = GetViewSelectionColor(
					FColor::White,
					*View,
					IsSelected(),
					IsHovered(),
					false,
					IsIndividuallySelected());

				DrawOrientedWireBox(
					PDI,
					LocalToWorld.GetOrigin(),
					LocalToWorld.GetScaledAxis(EAxis::X),
					LocalToWorld.GetScaledAxis(EAxis::Y),
					LocalToWorld.GetScaledAxis(EAxis::Z),
					Extent,
					Color,
					SDPG_World);
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = (IsShown(View) && IsSelected()) || GVoxelShowInvokers;
			Result.bDynamicRelevance = true;
			return Result;
		}
		virtual SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}
		virtual uint32 GetMemoryFootprint() const override
		{
			return sizeof(*this) + GetAllocatedSize();
		}
	};
	return new FBoxSceneProxy(this);
}

TSharedPtr<FVoxelInvoker> UVoxelBoxInvokerComponent::CreateInvoker() const
{
	return MakeVoxelShared<FVoxelBoxInvoker>(
		GetComponentTransform().ToInverseMatrixWithScale(),
		FVoxelBox(-Extent, Extent),
		this);
}

FBoxSphereBounds UVoxelBoxInvokerComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FBox(-Extent, Extent)).TransformBy(LocalToWorld);
}