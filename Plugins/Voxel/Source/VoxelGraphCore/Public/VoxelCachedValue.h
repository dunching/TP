// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelTask.h"
#include "VoxelQuery.h"

class VOXELGRAPHCORE_API FVoxelCachedValueStateBase
{
public:
	virtual ~FVoxelCachedValueStateBase() = default;

	virtual FVoxelPinType GetType() const = 0;
	virtual FVoxelFutureValue Get(FVoxelDependencyTracker& DependencyTracker) = 0;
};

class VOXELGRAPHCORE_API FVoxelCachedValue
{
public:
	FVoxelCachedValue() = default;
	FORCEINLINE explicit FVoxelCachedValue(const TSharedRef<FVoxelCachedValueStateBase>& State)
		: State(State)
	{
	}

	FORCEINLINE bool IsValid() const
	{
		return State.IsValid();
	}
	FORCEINLINE FVoxelFutureValue Get(FVoxelDependencyTracker& DependencyTracker) const
	{
		return State->Get(DependencyTracker);
	}

protected:
	TSharedPtr<FVoxelCachedValueStateBase> State;
};

template<typename T>
class TVoxelCachedValue : public FVoxelCachedValue
{
public:
	TVoxelCachedValue() = default;
	FORCEINLINE explicit TVoxelCachedValue(const TSharedRef<FVoxelCachedValueStateBase>& State)
		: FVoxelCachedValue(State)
	{
		checkVoxelSlow(State->GetType().CanBeCastedTo<T>());
	}

	FORCEINLINE TVoxelFutureValue<T> Get(FVoxelDependencyTracker& DependencyTracker) const
	{
		return TVoxelFutureValue<T>(State->Get(DependencyTracker));
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELGRAPHCORE_API FVoxelCachedValueFactoryBase
{
public:
	FVoxelCachedValueFactoryBase(
		const TSharedRef<const FVoxelComputeValue>& Compute,
		const FVoxelPinType& Type,
		const FName Name)
		: Compute(Compute)
		, Type(Type)
		, Name(Name)
		, Referencer(MakeVoxelShared<FVoxelTaskReferencer>(FVoxelUtilities::AppendName(TEXT("FVoxelCachedValue"), Name)))
	{
	}

protected:
	const TSharedRef<const FVoxelComputeValue> Compute;
	const FVoxelPinType Type;
	const FName Name;
	const TSharedRef<FVoxelTaskReferencer> Referencer;

	FVoxelTaskPriority PrivatePriority;

	TSharedRef<FVoxelCachedValueStateBase> ComputeState(
		const TSharedRef<FVoxelQueryContext>& Context,
		const TSharedRef<const FVoxelQueryParameters>& Parameters,
		FVoxelGetAllocatedSize&& GetAllocatedSize) const;
};

class FVoxelCachedValueFactory : public FVoxelCachedValueFactoryBase
{
public:
	using FVoxelCachedValueFactoryBase::FVoxelCachedValueFactoryBase;

	FORCEINLINE FVoxelCachedValueFactory& Priority(const FVoxelTaskPriority& NewPriority)
	{
		PrivatePriority = NewPriority;
		return *this;
	}

	template<typename RefType>
	FORCEINLINE FVoxelCachedValueFactory& AddRef(const RefType& Ref)
	{
		Referencer->AddRef(Ref);
		return *this;
	}

	FORCEINLINE FVoxelCachedValue Compute(
		const TSharedRef<FVoxelQueryContext>& Context,
		const TSharedRef<const FVoxelQueryParameters>& Parameters,
		FVoxelGetAllocatedSize&& GetAllocatedSize) const
	{
		return FVoxelCachedValue(this->ComputeState(
			Context,
			Parameters,
			MoveTemp(GetAllocatedSize)));
	}
};

template<typename T>
class TVoxelCachedValueFactory : public FVoxelCachedValueFactoryBase
{
public:
	explicit TVoxelCachedValueFactory(const FVoxelCachedValueFactory& Factory)
		: FVoxelCachedValueFactoryBase(Factory)
	{
		checkVoxelSlow(Type.CanBeCastedTo<T>());
	}
	TVoxelCachedValueFactory(const FName Name, TVoxelComputeValue<T>&& Compute)
		: FVoxelCachedValueFactoryBase(
			MakeVoxelShared<FVoxelComputeValue>([Compute = MoveTemp(Compute)](const FVoxelQuery& Query) -> FVoxelFutureValue
			{
				return Compute(Query);
			}),
			FVoxelPinType::Make<T>(),
			Name)
	{
	}

	FORCEINLINE TVoxelCachedValueFactory& Priority(const FVoxelTaskPriority& NewPriority)
	{
		PrivatePriority = NewPriority;
		return *this;
	}

	template<typename RefType>
	FORCEINLINE TVoxelCachedValueFactory& AddRef(const RefType& Ref)
	{
		Referencer->AddRef(Ref);
		return *this;
	}

	FORCEINLINE TVoxelCachedValue<T> Compute(
		const TSharedRef<FVoxelQueryContext>& Context,
		const TSharedRef<const FVoxelQueryParameters>& Parameters) const
	{
		return TVoxelCachedValue<T>(this->ComputeState(Context, Parameters, [](const FVoxelRuntimePinValue& Value)
		{
			return Value.Get<T>().GetAllocatedSize() + sizeof(T);
		}));
	}
};