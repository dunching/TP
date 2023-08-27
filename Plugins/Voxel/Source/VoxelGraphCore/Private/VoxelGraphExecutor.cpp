// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphExecutor.h"
#include "VoxelGraph.h"
#include "VoxelGraphInstance.h"
#include "VoxelGraphCompilerUtilities.h"
#include "VoxelRootNode.h"
#include "VoxelDependency.h"
#include "VoxelParameterContainer.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

VOXEL_CONSOLE_COMMAND(
	ForceTranslateAll,
	"voxel.ForceTranslateAll",
	"")
{
	GVoxelGraphExecutorManager->ForceTranslateAll();
}

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelGraphExecutor);
DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelGraphExecutorInfo);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelGraphExecutorManager* GVoxelGraphExecutorManager = nullptr;

VOXEL_RUN_ON_STARTUP_GAME(CreateVoxelGraphExecutorManager)
{
	GVoxelGraphExecutorManager = new FVoxelGraphExecutorManager();

	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddLambda([]
	{
		GVoxelGraphExecutorManager->CleanupOldExecutors(10.f);
	});

	GOnVoxelModuleUnloaded_DoCleanup.AddLambda([]
	{
		GVoxelGraphExecutorManager->CleanupOldExecutors(-1.f);
	});
}

DEFINE_UNIQUE_VOXEL_ID(FVoxelGraphExecutorGlobalId);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFutureValue FVoxelGraphExecutor::Execute(const FVoxelQuery& Query) const
{
	if (!RootNode)
	{
		// Empty graph
		return {};
	}
	return RootNode->GetNodeRuntime().Get(RootNode->ValuePin, Query.EnterScope(*RootNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGraphExecutorManager::Tick()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_USE_NAMESPACE(Graph);

	FCompilationScope CompilationScope;
	// Group all the invalidate calls
	const FVoxelDependencyInvalidationScope DependencyInvalidationScope;

	TSharedPtr<FExecutorRef> ExecutorRef;
	while (ExecutorRefsToUpdate.Dequeue(ExecutorRef))
	{
		if (!ExecutorRef->Executor_GameThread)
		{
			VOXEL_SCOPE_COUNTER_FORMAT("Recompiling %s", *ExecutorRef->Ref.ToString());
			ExecutorRef->SetExecutor(FCompilerUtilities::CreateExecutor(ExecutorRef->Ref));
		}
	}

	CleanupOldExecutors(30.f);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelGraphExecutorManager::GetReferencerName() const
{
	return "FVoxelGraphExecutorManager";
}

void FVoxelGraphExecutorManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	for (const auto& It : MakeCopy(ExecutorRefs_GameThread))
	{
		const TSharedPtr<const FVoxelGraphExecutorInfo> ExecutorInfo = It.Value->ExecutorInfo_GameThread;
		if (!ExecutorInfo)
		{
			continue;
		}

		for (const FObjectKey& ObjectKey : ExecutorInfo->ReferencedObjects)
		{
			UObject* Object = ObjectKey.ResolveObjectPtr();
			if (!Object)
			{
				continue;
			}

			Collector.AddReferencedObject(Object);
			ensure(Object);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGraphExecutorManager::RecompileAll()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_USE_NAMESPACE(Graph);
	check(IsInGameThread());

	const double StartTime = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		LOG_VOXEL(Log, "RecompileAll took %fs", FPlatformTime::Seconds() - StartTime);
	};

	const FCompilationScope CompilationScope;
	// Group all the invalidate calls
	const FVoxelDependencyInvalidationScope DependencyInvalidationScope;

	for (const auto& It : MakeCopy(ExecutorRefs_GameThread))
	{
		FExecutorRef& ExecutorRef = *It.Value;

		if (FCompilerUtilities::IsNodeDeleted(It.Key.Node))
		{
			ExecutorRefs_GameThread.Remove(It.Key);
			ExecutorRef.Dependency->Invalidate();
			continue;
		}

#if WITH_EDITOR
		if (FCompilerUtilities::RaiseOrphanWarnings_EditorOnly)
		{
			if (UVoxelGraph* Graph = It.Key.Node.GetGraph())
			{
				FCompilerUtilities::RaiseOrphanWarnings_EditorOnly(*Graph);
			}
		}
#endif

		ExecutorRef.SetExecutor(FCompilerUtilities::CreateExecutor(It.Key));
	}
}

void FVoxelGraphExecutorManager::ForceTranslateAll()
{
#if WITH_EDITOR
	VOXEL_FUNCTION_COUNTER();
	VOXEL_USE_NAMESPACE(Graph);
	check(IsInGameThread());

	if (!FCompilerUtilities::ForceTranslateGraph_EditorOnly)
	{
		return;
	}

	const double StartTime = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		LOG_VOXEL(Log, "ForceTranslateAll took %fs", FPlatformTime::Seconds() - StartTime);
	};

	TSet<UVoxelGraph*> Graphs;
	for (const auto& It : ExecutorRefs_GameThread)
	{
		if (UVoxelGraph* Graph = It.Key.Node.GetGraph())
		{
			Graphs.Add(Graph);
		}
	}

	for (UVoxelGraph* Graph : Graphs)
	{
		FCompilerUtilities::ForceTranslateGraph_EditorOnly(*Graph);
	}
#endif
}

void FVoxelGraphExecutorManager::CleanupOldExecutors(const double TimeInSeconds)
{
	VOXEL_FUNCTION_COUNTER();

	const double Time = FPlatformTime::Seconds();
	for (auto It = ExecutorRefs_GameThread.CreateIterator(); It; ++It)
	{
		if (It.Value().IsUnique())
		{
			It.RemoveCurrent();
			continue;
		}

		if (It.Value()->LastUsedTime + TimeInSeconds < Time)
		{
			It.Value()->Executor_GameThread.Reset();
		}
	}
}

void FVoxelGraphExecutorManager::OnGraphChanged_GameThread(const UVoxelGraphInterface& Graph)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_USE_NAMESPACE(Graph);
	check(IsInGameThread());

	const FCompilationScope CompilationScope;
	// Group all the invalidate calls
	const FVoxelDependencyInvalidationScope DependencyInvalidationScope;

	for (const auto& It : MakeCopy(ExecutorRefs_GameThread))
	{
		FExecutorRef& ExecutorRef = *It.Value;

		const bool bHasMatch = INLINE_LAMBDA
		{
			if (const TSharedPtr<const FVoxelGraphExecutorInfo> ExecutorInfo = ExecutorRef.ExecutorInfo_GameThread)
			{
				if (ExecutorInfo->ReferencedGraphs.Contains(&Graph))
				{
					return true;
				}
			}

			// Check the hierarchy for any potential match
			const UVoxelGraphInterface* ExecutorGraph = ExecutorRef.Ref.Node.Graph.Get();
			while (ExecutorGraph)
			{
				if (ExecutorGraph == &Graph)
				{
					return true;
				}

				const UVoxelGraphInstance* Instance = Cast<UVoxelGraphInstance>(ExecutorGraph);
				if (!Instance)
				{
					return false;
				}

				Instance->ParameterContainer->FixupProvider();
				ExecutorGraph = Instance->ParameterContainer->GetTypedProvider<UVoxelGraphInterface>();
			}

			return false;
		};

		if (!bHasMatch)
		{
			continue;
		}

		if (FCompilerUtilities::IsNodeDeleted(It.Key.Node))
		{
			ExecutorRefs_GameThread.Remove(It.Key);
			ExecutorRef.Dependency->Invalidate();
			continue;
		}

		const TSharedPtr<const FVoxelGraphExecutor> NewExecutor = FCompilerUtilities::CreateExecutor(It.Key);
		const TSharedPtr<const FVoxelGraphExecutorInfo> OldExecutorInfo = ExecutorRef.ExecutorInfo_GameThread;

		if (NewExecutor.IsValid() != OldExecutorInfo.IsValid())
		{
			LOG_VOXEL(Log, "Updating %s: compilation status changed", *It.Key.ToString());
			ExecutorRef.SetExecutor(NewExecutor);
			continue;
		}

		if (!NewExecutor.IsValid())
		{
			// Failed to compile
			continue;
		}

#if WITH_EDITOR
		FString Diff;
		if (OldExecutorInfo->Graph_EditorOnly->Identical(*NewExecutor->Info->Graph_EditorOnly, &Diff))
		{
			// No changes
			continue;
		}
		LOG_VOXEL(Log, "Updating %s: %s", *It.Key.ToString(), *Diff);
#endif

		ExecutorRef.SetExecutor(NewExecutor);
	}
}

TSharedRef<const FVoxelComputeValue> FVoxelGraphExecutorManager::MakeCompute_GameThread(
	const FVoxelPinType Type,
	const FVoxelGraphPinRef& Ref,
	const bool bReturnExecutor)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());
	check(Type.IsValid());
	ensure(!Type.IsWildcard());

	const TSharedRef<FExecutorRef> ExecutorRef = MakeExecutorRef_GameThread(Ref);

	return MakeVoxelShared<FVoxelComputeValue>([=](const FVoxelQuery& Query) -> FVoxelFutureValue
	{
		// Always add dependency, especially if we failed to compile
		Query.GetDependencyTracker().AddDependency(ExecutorRef->Dependency);

		const TVoxelFutureValue<FVoxelGraphExecutorRef> FutureExecutor = ExecutorRef->GetExecutor();

		if (bReturnExecutor)
		{
			return
				MakeVoxelTask()
				.Dependency(FutureExecutor)
				.Execute<FVoxelGraphExecutorRef>([=]
				{
					const TSharedRef<FVoxelGraphExecutorRef> Result = MakeVoxelShared<FVoxelGraphExecutorRef>();
					Result->Executor = FutureExecutor.Get_CheckCompleted().Executor;
					return Result;
				});
		}

		// Always create a task to check Value's type
		return
			MakeVoxelTask()
			.Dependency(FutureExecutor)
			.Execute(Type, [=]() -> FVoxelFutureValue
			{
				const TSharedPtr<const FVoxelGraphExecutor> Executor = FutureExecutor.Get_CheckCompleted().Executor;
				if (!Executor)
				{
					// Failed to compile
					return {};
				}

				FVoxelTaskReferencer::Get().AddExecutor(Executor.ToSharedRef());
				return Executor->Execute(Query);
			});
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelGraphExecutorManager::FExecutorRef::FExecutorRef(const FVoxelGraphPinRef& Ref)
	: Ref(Ref)
	, Dependency(FVoxelDependency::Create(STATIC_FNAME("GraphExecutor"), *Ref.ToString()))
{
}

TVoxelFutureValue<FVoxelGraphExecutorRef> FVoxelGraphExecutorManager::FExecutorRef::GetExecutor()
{
	LastUsedTime = FPlatformTime::Seconds();

	if (IsInGameThreadFast())
	{
		if (!Executor_GameThread.IsSet())
		{
			VOXEL_USE_NAMESPACE(Graph);
			SetExecutor(FCompilerUtilities::CreateExecutor(Ref));
		}

		const TSharedRef<FVoxelGraphExecutorRef> Struct = MakeVoxelShared<FVoxelGraphExecutorRef>();
		// Will be null if compilation failed
		Struct->Executor = Executor_GameThread.GetValue();
		return Struct;
	}

	VOXEL_SCOPE_LOCK(CriticalSection);

	if (const TSharedPtr<const FVoxelGraphExecutor> Executor = WeakExecutor_RequiresLock.Pin())
	{
		const TSharedRef<FVoxelGraphExecutorRef> Struct = MakeVoxelShared<FVoxelGraphExecutorRef>();
		Struct->Executor = Executor;
		return Struct;
	}

	if (!FutureValueState_RequiresLock.IsValid())
	{
		FutureValueState_RequiresLock = MakeVoxelShared<TVoxelFutureValueState<FVoxelGraphExecutorRef>>();
		GVoxelGraphExecutorManager->ExecutorRefsToUpdate.Enqueue(AsShared());
	}

	return TVoxelFutureValue<FVoxelGraphExecutorRef>(FVoxelFutureValue(FutureValueState_RequiresLock.ToSharedRef()));
}

void FVoxelGraphExecutorManager::FExecutorRef::SetExecutor(const TSharedPtr<const FVoxelGraphExecutor>& NewExecutor)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	GVoxelGraphExecutorManager->GlobalId = FVoxelGraphExecutorGlobalId::New();

	const TSharedPtr<const FVoxelGraphExecutorInfo> OldExecutorInfo = ExecutorInfo_GameThread;
	if (NewExecutor)
	{
		ExecutorInfo_GameThread = NewExecutor->Info;
		Executor_GameThread = NewExecutor;
	}
	else
	{
		ExecutorInfo_GameThread = nullptr;
		Executor_GameThread = nullptr;

		VOXEL_MESSAGE(Error, "{0} failed to compile", Ref);
	}

	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		WeakExecutor_RequiresLock = NewExecutor;

		if (FutureValueState_RequiresLock)
		{
			const TSharedRef<FVoxelGraphExecutorRef> Struct = MakeVoxelShared<FVoxelGraphExecutorRef>();
			Struct->Executor = NewExecutor;
			FutureValueState_RequiresLock->SetValue(Struct);
			FutureValueState_RequiresLock.Reset();
		}
	}

	// Outside of editor SetExecutor should never invalidate
	// Fully diff against old executor in case this SetExecutor is because it was GCed
#if WITH_EDITOR
	if (ExecutorInfo_GameThread.IsValid() != OldExecutorInfo.IsValid())
	{
		Dependency->Invalidate();
		return;
	}

	if (!ExecutorInfo_GameThread.IsValid())
	{
		return;
	}

	if (!ensure(ExecutorInfo_GameThread->Graph_EditorOnly) ||
		!ensure(OldExecutorInfo->Graph_EditorOnly))
	{
		return;
	}

	if (ExecutorInfo_GameThread->Graph_EditorOnly->Identical(*OldExecutorInfo->Graph_EditorOnly))
	{
		return;
	}

	Dependency->Invalidate();
#endif
}

TSharedRef<FVoxelGraphExecutorManager::FExecutorRef> FVoxelGraphExecutorManager::MakeExecutorRef_GameThread(const FVoxelGraphPinRef& Ref)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (const TSharedPtr<FExecutorRef> ExecutorRef = ExecutorRefs_GameThread.FindRef(Ref))
	{
		return ExecutorRef.ToSharedRef();
	}

	const TSharedRef<FExecutorRef> ExecutorRef = MakeVoxelShared<FExecutorRef>(Ref);
	ExecutorRefs_GameThread.Add(Ref, ExecutorRef);

	if (GVoxelBypassTaskQueue)
	{
		VOXEL_USE_NAMESPACE(Graph);
		ExecutorRef->SetExecutor(FCompilerUtilities::CreateExecutor(ExecutorRef->Ref));
	}

	return ExecutorRef;
}