// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelTask.h"

class FVoxelQueryContext;
extern VOXELGRAPHCORE_API const uint32 GVoxelTaskGroupTLS;

class VOXELGRAPHCORE_API FVoxelTaskGroup : public TSharedFromThis<FVoxelTaskGroup>
{
public:
	const FName Name;
	const bool bIsSynchronous;
	const bool bParallelTasks;
	const FVoxelTaskPriority Priority;
	const TSharedRef<FVoxelQueryContext> Context;
	const TSharedRef<FVoxelTaskReferencer> Referencer;

	static TSharedRef<FVoxelTaskGroup> Create(
		FName Name,
		const FVoxelTaskPriority& Priority,
		const TSharedRef<FVoxelQueryContext>& Context,
		const TSharedRef<FVoxelTaskReferencer>& Referencer);

	static TSharedRef<FVoxelTaskGroup> CreateSynchronous(
		FName Name,
		const TSharedRef<FVoxelQueryContext>& Context,
		const TSharedRef<FVoxelTaskReferencer>& Referencer);

	static TSharedRef<FVoxelTaskGroup> CreateSynchronous(
		const TSharedRef<FVoxelQueryContext>& Context);

public:
	static bool TryRunSynchronouslyGeneric(
		const TSharedRef<FVoxelQueryContext>& Context,
		TFunctionRef<void()> Lambda,
		FString* OutError = nullptr);

	template<typename LambdaType, typename T = typename decltype(DeclVal<LambdaType>()())::Type>
	static TOptional<T> TryRunSynchronously(
		const TSharedRef<FVoxelQueryContext>& Context,
		LambdaType&& Lambda,
		FString* OutError = nullptr)
	{
		TVoxelFutureValue<T> Future;

		if (!FVoxelTaskGroup::TryRunSynchronouslyGeneric(Context, [&]
			{
				Future = Lambda();
			}, OutError))
		{
			return {};
		}

		if (!ensure(Future.IsValid()) ||
			!ensure(Future.IsComplete()))
		{
			return {};
		}

		return Future.Get_CheckCompleted();
	}

	bool TryRunSynchronously(FString* OutError);
	bool TryRunSynchronously_Ensure();

public:
	FORCEINLINE FVoxelTaskReferencer& GetReferencer() const
	{
		return *Referencer;
	}

	void ProcessTask(TVoxelUniquePtr<FVoxelTask> TaskPtr);
	void AddPendingTask(TVoxelUniquePtr<FVoxelTask> TaskPtr);
	void OnDependencyComplete(FVoxelTask& Task);

	void LogTasks() const;

public:
	TVoxelAtomic<const void*> AsyncProcessor = nullptr;

	FORCEINLINE bool HasGameTasks() const { return !GameTasks.IsEmpty(); }
	FORCEINLINE bool HasRenderTasks() const { return !RenderTasks.IsEmpty(); }
	FORCEINLINE bool HasAsyncTasks() const { return !AsyncTasks.IsEmpty(); }

	void ProcessGameTasks();
	void ProcessRenderTasks(FRDGBuilder& GraphBuilder);
	void ProcessAsyncTasks();

public:
	FORCEINLINE bool ShouldExit() const
	{
		return !bIsSynchronous && IsSharedFromThisUnique(this);
	}
	FORCEINLINE static FVoxelTaskGroup& Get()
	{
		void* TLSValue = FPlatformTLS::GetTlsValue(GVoxelTaskGroupTLS);
		checkVoxelSlow(TLSValue);
		return *static_cast<FVoxelTaskGroup*>(TLSValue);
	}
	FORCEINLINE static FVoxelTaskGroup* TryGet()
	{
		void* TLSValue = FPlatformTLS::GetTlsValue(GVoxelTaskGroupTLS);
		return static_cast<FVoxelTaskGroup*>(TLSValue);
	}

private:
	TQueue<TVoxelUniquePtr<FVoxelTask>, EQueueMode::Mpsc> GameTasks;
	TQueue<TVoxelUniquePtr<FVoxelTask>, EQueueMode::Mpsc> RenderTasks;
	TQueue<TVoxelUniquePtr<FVoxelTask>, EQueueMode::Mpsc> AsyncTasks;

	mutable FVoxelFastCriticalSection PendingTasksCriticalSection;
	TVoxelSparseArray<TVoxelUniquePtr<FVoxelTask>, FVoxelPendingTaskId> PendingTasks_RequiresLock;

	FVoxelTaskGroup(
		FName Name,
		bool bIsSynchronous,
		const FVoxelTaskPriority& Priority,
		const TSharedRef<FVoxelQueryContext>& Context,
		const TSharedRef<FVoxelTaskReferencer>& Referencer);
};

struct VOXELGRAPHCORE_API FVoxelTaskGroupScope
{
public:
	const TSharedRef<FVoxelTaskGroup> Group;

	FORCEINLINE explicit FVoxelTaskGroupScope(FVoxelTaskGroup& Group)
		: Group(Group.AsShared())
	{
		PreviousTLS = FPlatformTLS::GetTlsValue(GVoxelTaskGroupTLS);
		FPlatformTLS::SetTlsValue(GVoxelTaskGroupTLS, &Group);
	}
	FORCEINLINE ~FVoxelTaskGroupScope()
	{
		FPlatformTLS::SetTlsValue(GVoxelTaskGroupTLS, PreviousTLS);
	}

private:
	void* PreviousTLS = nullptr;
};