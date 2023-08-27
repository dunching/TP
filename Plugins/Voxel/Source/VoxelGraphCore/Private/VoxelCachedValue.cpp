// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelCachedValue.h"
#include "VoxelTaskGroup.h"
#include "VoxelDependency.h"

class VOXELGRAPHCORE_API FVoxelCachedValueState final
	: public FVoxelCachedValueStateBase
	, public TSharedFromThis<FVoxelCachedValueState>
{
public:
	const TSharedRef<const FVoxelComputeValue> ComputeLambda;
	const FVoxelGetAllocatedSize GetAllocatedSize;
	const FName Name;
	const FVoxelPinType Type;
	const FVoxelTaskPriority Priority;
	const TSharedRef<FVoxelQueryContext> Context;
	const TSharedRef<const FVoxelQueryParameters> Parameters;
	const TSharedRef<FVoxelTaskReferencer> Referencer;

	VOXEL_COUNT_INSTANCES();
	UE_NONCOPYABLE(FVoxelCachedValueState);
	virtual ~FVoxelCachedValueState() override;

	virtual FVoxelPinType GetType() const override
	{
		return Type;
	}
	virtual FVoxelFutureValue Get(FVoxelDependencyTracker& DependencyTracker) override;

private:
	FVoxelCachedValueState(
		const TSharedRef<const FVoxelComputeValue>& ComputeLambda,
		FVoxelGetAllocatedSize&& GetAllocatedSize,
		const FName Name,
		const FVoxelPinType& Type,
		const FVoxelTaskPriority& Priority,
		const TSharedRef<FVoxelQueryContext>& Context,
		const TSharedRef<const FVoxelQueryParameters>& Parameters,
		const TSharedRef<FVoxelTaskReferencer>& Referencer)
		: ComputeLambda(ComputeLambda)
		, GetAllocatedSize(MoveTemp(GetAllocatedSize))
		, Name(FVoxelUtilities::AppendName(TEXT("CachedValue - "), Name))
		, Type(Type)
		, Priority(Priority)
		, Context(Context)
		, Parameters(Parameters)
		, Referencer(Referencer)
	{
	}

	FVoxelFastCriticalSection CriticalSection;

	int64 AllocatedSize = 0;
	FVoxelFutureValue Value;
	TSharedPtr<FVoxelDependency> Dependency;
	TSharedPtr<FVoxelTaskGroup> Group;
	TSharedPtr<FVoxelDependencyTracker> DependencyTracker;

	void OnComputed(
		const FVoxelFutureValue& ComputedValue,
		const TSharedRef<FVoxelDependencyTracker>& NewDependencyTracker);

	friend class FVoxelCachedValueFactoryBase;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelCachedValueState);

FVoxelCachedValueState::~FVoxelCachedValueState()
{
	Voxel_AddAmountToDynamicStat(Name, -AllocatedSize);
	AllocatedSize = 0;
}

FVoxelFutureValue FVoxelCachedValueState::Get(FVoxelDependencyTracker& InDependencyTracker)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);
	ensure(Value.IsValid() == Dependency.IsValid());

	if (Value.IsValid())
	{
		InDependencyTracker.AddDependency(Dependency.ToSharedRef());
		return Value;
	}

	ensure(!Dependency);
	Dependency = FVoxelDependency::Create(STATIC_FNAME("CachedValue"), Name);
	InDependencyTracker.AddDependency(Dependency.ToSharedRef());

	ensure(!Group);
	Group = FVoxelTaskGroup::Create(Name, Priority, Context, Referencer);

	DependencyTracker.Reset();

	FVoxelTaskGroupScope Scope(*Group);

	Value =
		MakeVoxelTask(STATIC_FNAME("FVoxelCachedValueState"))
		.NeverBypassQueue()
		.Execute(Type, [this, This = AsShared()]() -> FVoxelFutureValue
		{
			const TSharedRef<FVoxelDependencyTracker> NewDependencyTracker = FVoxelDependencyTracker::Create(Name);
			const FVoxelQuery Query = FVoxelQuery::Make(
				Context,
				Parameters,
				NewDependencyTracker);

			const FVoxelFutureValue FutureValue = (*ComputeLambda)(Query);
			if (!FutureValue.IsValid())
			{
				OnComputed(FutureValue, NewDependencyTracker);
				return {};
			}

			return
				MakeVoxelTask(STATIC_FNAME("FVoxelCachedValueState_OnComputed"))
				.Dependency(FutureValue)
				.Execute(Type, [=]
				{
					(void)This;
					OnComputed(FutureValue, NewDependencyTracker);
					return FutureValue;
				});
		});

	ensure(Value.IsValid());
	return Value;
}

void FVoxelCachedValueState::OnComputed(
	const FVoxelFutureValue& ComputedValue,
	const TSharedRef<FVoxelDependencyTracker>& NewDependencyTracker)
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	check(Dependency);
	check(!ComputedValue.IsValid() || ComputedValue.IsComplete());

	ensure(Group);
	Group = {};

	ensure(!DependencyTracker);
	DependencyTracker = NewDependencyTracker;

	if (!DependencyTracker->TrySetOnInvalidated(MakeWeakPtrLambda(this,
		[this, DependencyPtr = Dependency.Get()]
		{
			VOXEL_SCOPE_LOCK(CriticalSection);

			if (!ensure(DependencyPtr == Dependency.Get()) ||
				!ensure(DependencyTracker->IsInvalidated()))
			{
				return;
			}

			Dependency->Invalidate();

			Value = {};
			Dependency = {};
			ensure(!Group);
			DependencyTracker = {};
		})))
	{
		ensure(DependencyTracker->IsInvalidated());
		Dependency->Invalidate();

		Value = {};
		Dependency = {};
		ensure(!Group);
		DependencyTracker = {};
	}

	if (GetAllocatedSize)
	{
		Voxel_AddAmountToDynamicStat(Name, -AllocatedSize);
		if (ComputedValue.IsValid())
		{
			AllocatedSize = GetAllocatedSize(ComputedValue.GetValue_CheckCompleted());
		}
		else
		{
			AllocatedSize = 0;
		}
		Voxel_AddAmountToDynamicStat(Name, AllocatedSize);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelCachedValueStateBase> FVoxelCachedValueFactoryBase::ComputeState(
	const TSharedRef<FVoxelQueryContext>& Context,
	const TSharedRef<const FVoxelQueryParameters>& Parameters,
	FVoxelGetAllocatedSize&& GetAllocatedSize) const
{
	return MakeVoxelShareable(new (GVoxelMemory) FVoxelCachedValueState(
		Compute,
		MoveTemp(GetAllocatedSize),
		Name,
		Type,
		PrivatePriority,
		Context,
		Parameters,
		Referencer));
}