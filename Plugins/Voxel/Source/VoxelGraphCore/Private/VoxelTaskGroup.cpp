// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelTaskGroup.h"
#include "VoxelTaskExecutor.h"
#include "VoxelQuery.h"

const uint32 GVoxelTaskGroupTLS = FPlatformTLS::AllocTlsSlot();

TSharedRef<FVoxelTaskGroup> FVoxelTaskGroup::Create(
	const FName Name,
	const FVoxelTaskPriority& Priority,
	const TSharedRef<FVoxelQueryContext>& Context,
	const TSharedRef<FVoxelTaskReferencer>& Referencer)
{
	const TSharedRef<FVoxelTaskGroup> Group = MakeVoxelShareable(new (GVoxelMemory) FVoxelTaskGroup(
		Name,
		false,
		Priority,
		Context,
		Referencer));
	GVoxelTaskExecutor->AddGroup(Group);
	return Group;
}

TSharedRef<FVoxelTaskGroup> FVoxelTaskGroup::CreateSynchronous(
	const FName Name,
	const TSharedRef<FVoxelQueryContext>& Context,
	const TSharedRef<FVoxelTaskReferencer>& Referencer)
{
	return MakeVoxelShareable(new (GVoxelMemory) FVoxelTaskGroup(
		Name,
		true,
		{},
		Context,
		Referencer));
}

TSharedRef<FVoxelTaskGroup> FVoxelTaskGroup::CreateSynchronous(
		const TSharedRef<FVoxelQueryContext>& Context)
{
	return CreateSynchronous(
		STATIC_FNAME("Synchronous"),
		Context,
		MakeVoxelShared<FVoxelTaskReferencer>(STATIC_FNAME("Synchronous")));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelTaskGroup::TryRunSynchronouslyGeneric(
	const TSharedRef<FVoxelQueryContext>& Context,
	const TFunctionRef<void()> Lambda,
	FString* OutError)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FVoxelTaskGroup> Group = CreateSynchronous(Context);
	FVoxelTaskGroupScope Scope(*Group);

	Lambda();

	return Group->TryRunSynchronously(OutError);
}

bool FVoxelTaskGroup::TryRunSynchronously(FString* OutError)
{
	VOXEL_FUNCTION_COUNTER();
	check(bIsSynchronous);

	const bool bRunGameTasks = IsInGameThread();
	const bool bRunRenderTasks = IsInRenderingThread();

	{
		TVoxelUniquePtr<FVoxelTask> Task;
		while (
			(bRunGameTasks && GameTasks.Dequeue(Task)) ||
			(bRunRenderTasks && RenderTasks.Dequeue(Task)) ||
			AsyncTasks.Dequeue(Task))
		{
			Task->Execute();
		}
	}

	bool bSuccess = true;
	TArray<FString> GameTaskNames;
	TArray<FString> RenderTaskNames;
	TArray<FString> PendingTaskNames;

	{
		TVoxelUniquePtr<FVoxelTask> Task;
		while (GameTasks.Dequeue(Task))
		{
			ensure(!bRunGameTasks);
			bSuccess = false;

			if (OutError)
			{
				GameTaskNames.Add(Task->Name.ToString());
			}
			else
			{
				break;
			}
		}
	}
	{
		TVoxelUniquePtr<FVoxelTask> Task;
		while (RenderTasks.Dequeue(Task))
		{
			ensure(!bRunRenderTasks);
			bSuccess = false;

			if (OutError)
			{
				RenderTaskNames.Add(Task->Name.ToString());
			}
			else
			{
				break;
			}
		}
	}
	{
		TVoxelUniquePtr<FVoxelTask> Task;
		while (AsyncTasks.Dequeue(Task))
		{
			ensure(false);
			bSuccess = false;
		}
	}

	{
		VOXEL_SCOPE_LOCK(PendingTasksCriticalSection);
		for (const TVoxelUniquePtr<FVoxelTask>& PendingTask : PendingTasks_RequiresLock)
		{
			bSuccess = false;

			if (OutError)
			{
				PendingTaskNames.Add(PendingTask->Name.ToString());
			}
			else
			{
				break;
			}
		}
	}

	if (bSuccess)
	{
		return true;
	}

	if (OutError)
	{
		*OutError = "Failed to process tasks synchronously. Tasks left:\n";

		if (GameTaskNames.Num() > 0)
		{
			*OutError += "Game tasks: " + FString::Join(GameTaskNames, TEXT(",")) + "\n";
		}
		if (RenderTaskNames.Num() > 0)
		{
			*OutError += "Render tasks: " + FString::Join(RenderTaskNames, TEXT(",")) + "\n";
		}
		if (PendingTaskNames.Num() > 0)
		{
			*OutError += "Pending tasks: " + FString::Join(PendingTaskNames, TEXT(",")) + "\n";
		}

		OutError->RemoveFromEnd("\n");
	}

	return false;
}

bool FVoxelTaskGroup::TryRunSynchronously_Ensure()
{
	ensure(IsInGameThread());
	FString Error;
	return ensureMsgf(TryRunSynchronously(&Error), TEXT("Failed to run %s synchronously: %s"), *Name.ToString(), *Error);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTaskGroup::ProcessTask(TVoxelUniquePtr<FVoxelTask> TaskPtr)
{
	checkVoxelSlow(TaskPtr->NumDependencies.GetValue() == 0);

	switch (TaskPtr->Thread)
	{
	default: VOXEL_ASSUME(false);
	case EVoxelTaskThread::GameThread:
	{
		if (IsInGameThreadFast())
		{
			FVoxelTaskGroupScope Scope(*this);
			TaskPtr->Execute();
		}
		else
		{
			GameTasks.Enqueue(MoveTemp(TaskPtr));
		}
	}
	break;
	case EVoxelTaskThread::RenderThread:
	{
		// Enqueue to ensure commands enqueued before this will be run before it
		ENQUEUE_RENDER_COMMAND(FVoxelTaskProcessor_ProcessTask)(
			MakeWeakPtrLambda(this, [this, TaskPtrPtr = MakeUniqueCopy(MoveTemp(TaskPtr))](FRHICommandList& RHICmdList)
			{
				RenderTasks.Enqueue(MoveTemp(*TaskPtrPtr));
			}));
	}
	break;
	case EVoxelTaskThread::AsyncThread:
	{
		AsyncTasks.Enqueue(MoveTemp(TaskPtr));

		if (!AsyncProcessor.Load(std::memory_order_relaxed))
		{
			VOXEL_SCOPE_COUNTER("Trigger");
			GVoxelTaskExecutor->Event.Trigger();
		}
	}
	break;
	}
}

void FVoxelTaskGroup::AddPendingTask(TVoxelUniquePtr<FVoxelTask> TaskPtr)
{
	FVoxelTask& Task = *TaskPtr;
	checkVoxelSlow(Task.NumDependencies.GetValue() > 0);
	checkVoxelSlow(!Task.PendingTaskId.IsValid());

	VOXEL_SCOPE_LOCK(PendingTasksCriticalSection);

	Task.PendingTaskId = PendingTasks_RequiresLock.Add(MoveTemp(TaskPtr));
}

void FVoxelTaskGroup::OnDependencyComplete(FVoxelTask& Task)
{
	const int32 NumDependenciesLeft = Task.NumDependencies.Decrement();
	checkVoxelSlow(NumDependenciesLeft >= 0);

	if (NumDependenciesLeft > 0)
	{
		return;
	}

	TVoxelUniquePtr<FVoxelTask> TaskPtr;
	{
		VOXEL_SCOPE_LOCK(PendingTasksCriticalSection);

		if (!ensure(PendingTasks_RequiresLock.IsValidIndex(Task.PendingTaskId)))
		{
			return;
		}

		TaskPtr = MoveTemp(PendingTasks_RequiresLock[Task.PendingTaskId]);

		PendingTasks_RequiresLock.RemoveAt(Task.PendingTaskId);
		Task.PendingTaskId = {};
	}
	checkVoxelSlow(TaskPtr.Get() == &Task);

	ProcessTask(MoveTemp(TaskPtr));
}

void FVoxelTaskGroup::LogTasks() const
{
	VOXEL_FUNCTION_COUNTER();

	LOG_VOXEL(Log, "%s", *Name.ToString());

	if (!GameTasks.IsEmpty())
	{
		LOG_VOXEL(Log, "\tHas queued Game Tasks");
	}
	if (!RenderTasks.IsEmpty())
	{
		LOG_VOXEL(Log, "\tHas queued Render Tasks");
	}
	if (!AsyncTasks.IsEmpty())
	{
		LOG_VOXEL(Log, "\tHas queued Async Tasks");
	}

	VOXEL_SCOPE_LOCK(PendingTasksCriticalSection);

	for (const TVoxelUniquePtr<FVoxelTask>& PendingTask : PendingTasks_RequiresLock)
	{
		LOG_VOXEL(Log, "\tPending task %s: %d dependencies", *PendingTask->Name.ToString(), PendingTask->NumDependencies.GetValue());
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTaskGroup::ProcessGameTasks()
{
	VOXEL_SCOPE_COUNTER_FNAME(Name);
	check(!bIsSynchronous);
	check(&Get() == this);
	const FVoxelQueryScope Scope(nullptr, &Context.Get());

	TVoxelUniquePtr<FVoxelTask> Task;
	while (
		!ShouldExit() &&
		GameTasks.Dequeue(Task))
	{
		Task->Execute();
	}
}

void FVoxelTaskGroup::ProcessRenderTasks(FRDGBuilder& GraphBuilder)
{
	VOXEL_SCOPE_COUNTER_FNAME(Name);
	check(!bIsSynchronous);
	check(&Get() == this);
	const FVoxelQueryScope Scope(nullptr, &Context.Get());

	TVoxelUniquePtr<FVoxelTask> Task;
	while (
		!ShouldExit() &&
		RenderTasks.Dequeue(Task))
	{
		RDG_EVENT_SCOPE(GraphBuilder, "%s", *Task->Name.ToString());

#if HAS_GPU_STATS
		static FDrawCallCategoryName DrawCallCategoryName;
		FRDGGPUStatScopeGuard StatScope(GraphBuilder, *Task->Name.ToString(), {}, nullptr, DrawCallCategoryName);
#endif

#if 0
		if (Task->Stat)
		{
			FVoxelShaderStatsScope::SetCallback([Stat = Task->Stat](int64 TimeInMicroSeconds, FName Name)
			{
				Stat->AddTime(
					Name == STATIC_FNAME("Readback")
					? FVoxelTaskStat::CopyGpuToCpu
					: FVoxelTaskStat::GpuCompute,
					TimeInMicroSeconds * 1000);
			});
		}
#endif

		Task->Execute();

#if 0
		if (Task->Stat)
		{
			FVoxelShaderStatsScope::SetCallback(nullptr);
		}
#endif
	}
}

void FVoxelTaskGroup::ProcessAsyncTasks()
{
	VOXEL_SCOPE_COUNTER_FNAME(Name);
	check(!bIsSynchronous);
	check(AsyncProcessor.Load());
	check(&Get() == this);
	const FVoxelQueryScope Scope(nullptr, &Context.Get());

	TVoxelUniquePtr<FVoxelTask> Task;
	while (
		!ShouldExit() &&
		AsyncTasks.Dequeue(Task))
	{
		Task->Execute();
	}
}

FVoxelTaskGroup::FVoxelTaskGroup(
	const FName Name,
	const bool bIsSynchronous,
	const FVoxelTaskPriority& Priority,
	const TSharedRef<FVoxelQueryContext>& Context,
	const TSharedRef<FVoxelTaskReferencer>& Referencer)
	: Name(Name)
	, bIsSynchronous(bIsSynchronous)
	, bParallelTasks(Context->RuntimeInfo->ShouldRunTasksInParallel())
	, Priority(Priority)
	, Context(Context)
	, Referencer(Referencer)
{
	VOXEL_FUNCTION_COUNTER();
	PendingTasks_RequiresLock.Reserve(512);
}