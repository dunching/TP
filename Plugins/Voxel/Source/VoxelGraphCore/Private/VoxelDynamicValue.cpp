// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelDynamicValue.h"
#include "VoxelDependency.h"
#include "VoxelTaskGroup.h"

class VOXELGRAPHCORE_API FVoxelDynamicValueState final
	: public FVoxelDynamicValueStateBase
	, public TSharedFromThis<FVoxelDynamicValueState>
{
public:
	const TSharedRef<const FVoxelComputeValue> ComputeLambda;
	const FName Name;
	const FVoxelPinType Type;
	const EVoxelTaskThread Thread;
	const FVoxelTaskPriority Priority;
	const TSharedRef<FVoxelQueryContext> Context;
	const TSharedRef<const FVoxelQueryParameters> Parameters;
	const TSharedRef<FVoxelTaskReferencer> Referencer;

	VOXEL_COUNT_INSTANCES();
	UE_NONCOPYABLE(FVoxelDynamicValueState);

	virtual FVoxelPinType GetType() const override
	{
		return Type;
	}
	virtual void SetOnChanged(FOnChanged&& NewOnChanged) const override
	{
		OnChangedImpl->SetOnChanged(MoveTemp(NewOnChanged));
	}

private:
	FVoxelDynamicValueState(
		const TSharedRef<const FVoxelComputeValue>& ComputeLambda,
		const FName Name,
		const FVoxelPinType& Type,
		const EVoxelTaskThread Thread,
		const FVoxelTaskPriority& Priority,
		const TSharedRef<FVoxelQueryContext>& Context,
		const TSharedRef<const FVoxelQueryParameters>& Parameters,
		const TSharedRef<FVoxelTaskReferencer>& Referencer)
		: ComputeLambda(ComputeLambda)
		, Name(FVoxelUtilities::AppendName(TEXT("DynamicValue - "), Name))
		, Type(Type)
		, Thread(Thread)
		, Priority(Priority)
		, Context(Context)
		, Parameters(Parameters)
		, Referencer(Referencer)
	{
	}

	FVoxelFastCriticalSection CriticalSection;
	TSharedPtr<FVoxelTaskGroup> Group;
	TSharedPtr<FVoxelDependencyTracker> DependencyTracker;
	int32 ValueCounter = 0;

	class FOnChangedImpl : public TSharedFromThis<FOnChangedImpl>
	{
	public:
		void SetOnChanged(FOnChanged&& NewOnChanged);
		void Execute(const FVoxelRuntimePinValue& NewValue, int32 NewValueCounter);

	private:
		FVoxelFastCriticalSection CriticalSection;
		FOnChanged OnChanged;
		int32 LastValueCounter = -1;
		FVoxelRuntimePinValue PendingValue;
	};
	const TSharedRef<FOnChangedImpl> OnChangedImpl = MakeVoxelShared<FOnChangedImpl>();

	void Compute_RequiresLock_WillUnlock();
	void OnComputed(
		FVoxelRuntimePinValue NewValue,
		const TSharedRef<FVoxelDependencyTracker>& NewDependencyTracker);

	friend class FVoxelDynamicValueFactoryBase;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelDynamicValueState);

void FVoxelDynamicValueState::FOnChangedImpl::SetOnChanged(FOnChanged&& NewOnChanged)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	ensure(!OnChanged);
	OnChanged = MoveTemp(NewOnChanged);

	if (!PendingValue.IsValid())
	{
		return;
	}

	const FVoxelRuntimePinValue PendingValueCopy = PendingValue;
	PendingValue = {};
	OnChanged(PendingValueCopy);
}

void FVoxelDynamicValueState::FOnChangedImpl::Execute(const FVoxelRuntimePinValue& NewValue, const int32 NewValueCounter)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	if (LastValueCounter > NewValueCounter)
	{
		// We already have a more recent value
		return;
	}
	LastValueCounter = NewValueCounter;

	if (!OnChanged)
	{
		PendingValue = NewValue;
		return;
	}

	OnChanged(NewValue);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDynamicValueState::Compute_RequiresLock_WillUnlock()
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(CriticalSection.IsLocked());

	if (State == EState::UpToDate)
	{
		State = EState::Outdated;
	}

	ensure(!Group);
	if (Thread == EVoxelTaskThread::GameThread &&
		IsInGameThread())
	{
		Group = FVoxelTaskGroup::CreateSynchronous(Name, Context, Referencer);
	}
	else
	{
		Group = FVoxelTaskGroup::Create(Name, Priority, Context, Referencer);
	}

	DependencyTracker.Reset();

	const FVoxelTaskGroupScope Scope(*Group);

	// In case the voxel task is ran immediately if Thread is GameThread
	CriticalSection.Unlock();

	MakeVoxelTask(STATIC_FNAME("FVoxelDynamicValueState_Setup"))
	.Thread(Thread)
	.Execute(MakeWeakPtrLambda(this, [this]
	{
		const TSharedRef<FVoxelDependencyTracker> NewDependencyTracker = FVoxelDependencyTracker::Create(Name);
		const FVoxelQuery Query = FVoxelQuery::Make(
			Context,
			Parameters,
			NewDependencyTracker);

		FVoxelFutureValue FutureValue = (*ComputeLambda)(Query);
		if (!FutureValue.IsValid())
		{
			FutureValue = FVoxelRuntimePinValue(Type);
		}

		MakeVoxelTask(STATIC_FNAME("FVoxelDynamicValueState_Wait"))
		.Thread(Thread)
		.Dependency(FutureValue)
		.Execute(MakeWeakPtrLambda(this, [this, NewDependencyTracker, FutureValue]
		{
			OnComputed(FutureValue.GetValue_CheckCompleted(), NewDependencyTracker);
		}));
	}));

	if (Scope.Group->bIsSynchronous)
	{
		Scope.Group->TryRunSynchronously_Ensure();
	}
}

void FVoxelDynamicValueState::OnComputed(
	FVoxelRuntimePinValue NewValue,
	const TSharedRef<FVoxelDependencyTracker>& NewDependencyTracker)
{
	VOXEL_FUNCTION_COUNTER();
	CriticalSection.Lock();

	if (!ensure(NewValue.GetType().CanBeCastedTo(Type)))
	{
		NewValue = FVoxelRuntimePinValue(Type);
	}

	ensure(State != EState::UpToDate);
	State = EState::UpToDate;

	ensure(Group);
	Group = {};

	ensure(!DependencyTracker);
	DependencyTracker = NewDependencyTracker;

	OnChangedImpl->Execute(NewValue, ValueCounter++);

	if (!DependencyTracker->TrySetOnInvalidated(MakeWeakPtrLambda(this,
		[this]
		{
			CriticalSection.Lock();

			if (State != EState::UpToDate ||
				!ensure(DependencyTracker) ||
				!ensure(DependencyTracker->IsInvalidated()))
			{
				CriticalSection.Unlock();
				return;
			}

			Compute_RequiresLock_WillUnlock();
		})))
	{
		// Already invalidated, recompute
		ensure(DependencyTracker->IsInvalidated());
		Compute_RequiresLock_WillUnlock();
	}
	else
	{
		CriticalSection.Unlock();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelDynamicValueStateBase> FVoxelDynamicValueFactoryBase::ComputeState(
	const TSharedRef<FVoxelQueryContext>& Context,
	const TSharedRef<const FVoxelQueryParameters>& Parameters) const
{
	const TSharedRef<FVoxelDynamicValueState> State = MakeVoxelShareable(new (GVoxelMemory) FVoxelDynamicValueState(
		Compute,
		Name,
		Type,
		PrivateThread,
		PrivatePriority,
		Context,
		Parameters,
		Referencer));

	State->CriticalSection.Lock();
	State->Compute_RequiresLock_WillUnlock();
	return State;
}