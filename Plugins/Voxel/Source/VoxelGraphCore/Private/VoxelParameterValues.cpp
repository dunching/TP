// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelParameterValues.h"
#include "VoxelTask.h"
#include "VoxelQuery.h"
#include "VoxelParameter.h"
#include "VoxelDependency.h"
#include "VoxelInlineGraph.h"
#include "VoxelNodeMessages.h"
#include "VoxelParameterView.h"
#include "VoxelParameterContainer.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelParameterValues);

TMap<FVoxelParameterPath, FName> GVoxelParameterGuidToDebugName;

TSharedRef<FVoxelParameterValues> FVoxelParameterValues::Create(const TWeakInterfacePtr<IVoxelParameterProvider>& Provider)
{
	check(IsInGameThread());

	// Will happen if asset is being deleted
	if (!ensureVoxelSlow(Provider.IsValid()))
	{
		return MakeVoxelShareable(new (GVoxelMemory) FVoxelParameterValues({}, nullptr));
	}

	const TSharedRef<FVoxelParameterValues> ParameterValues = MakeVoxelShareable(new(GVoxelMemory) FVoxelParameterValues(
		Provider.GetObject()->GetFName(),
		Provider));

	ParameterValues->BindOnChanged(Provider);
	ParameterValues->Update_GameThread();

	return ParameterValues;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFutureValue FVoxelParameterValues::FindParameter(
	const FVoxelPinType& Type,
	const FVoxelParameterPath& Path,
	const FVoxelQuery& Query) const
{
	VOXEL_FUNCTION_COUNTER();

	{
		VOXEL_SCOPE_LOCK(CriticalSection);
		if (const FParameterValue* ParameterValue = PathToValue_RequiresLock.Find(Path))
		{
			if (!ensureVoxelSlow(ParameterValue->Value.GetType() == Type))
			{
				VOXEL_MESSAGE(Error, "{0}: INTERNAL ERROR: Invalid parameter type: {1} vs {2}",
					Query,
					Type.ToString(),
					ParameterValue->Value.GetType().ToString());

				return FVoxelRuntimePinValue(Type);
			}

			Query.GetDependencyTracker().AddDependency(ParameterValue->Dependency.ToSharedRef());
			return ParameterValue->Value;
		}
	}

	return
		MakeVoxelTask(STATIC_FNAME("FindParameter"))
		.Thread(EVoxelTaskThread::GameThread)
		.Execute(Type, MakeWeakPtrLambda(this, [this, Type, Path, Query]() -> FVoxelFutureValue
		{
			return ConstCast(this)->FindParameter_GameThread(Type, Path, Query);
		}));
}

void FVoxelParameterValues::Tick()
{
	VOXEL_FUNCTION_COUNTER();

	if (!bUpdateQueued)
	{
		return;
	}
	bUpdateQueued = false;

	Update_GameThread();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelParameterValues::BindOnChanged(const TWeakInterfacePtr<IVoxelParameterProvider>& Provider)
{
	if (!ensure(Provider.IsValid()))
	{
		return;
	}

	if (BoundProviders.Contains(Provider))
	{
		return;
	}
	BoundProviders.Add(Provider);

	Provider->AddOnChanged(MakeWeakPtrDelegate(this, [this]
	{
		bUpdateQueued = true;
	}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelParameterValues::Update_GameThread()
{
	// Group all the invalidate calls
	const FVoxelDependencyInvalidationScope DependencyInvalidationScope;
	VOXEL_SCOPE_LOCK(CriticalSection);
	Update_GameThread_RequiresLock();
}

void FVoxelParameterValues::Update_GameThread_RequiresLock()
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelParameterValues::Update %s", *Name.ToString());
	check(IsInGameThread());
	check(CriticalSection.IsLocked());

	if (!ensure(RootProvider.IsValid()))
	{
		return;
	}

	const TSharedPtr<IVoxelParameterRootView> ParameterRootView = RootProvider->GetParameterView();
	if (!ensure(ParameterRootView))
	{
		return;
	}

	for (auto It = PathToValue_RequiresLock.CreateIterator(); It; ++It)
	{
		const FVoxelParameterPath& ParameterPath = It.Key();
		FParameterValue& ParameterValue = It.Value();

		const IVoxelParameterView* ParameterView = ParameterRootView->FindChild(ParameterPath);
		if (!ParameterView)
		{
			if (ensure(ParameterValue.Dependency))
			{
				// Ensure will fail if we failed to find a parameter we just added
				ParameterValue.Dependency->Invalidate();
			}

			It.RemoveCurrent();
			continue;
		}

		const FVoxelPinValue NewValue = ParameterView->GetValue();

		if (!ParameterValue.Dependency)
		{
			if (FPlatformMisc::IsDebuggerPresent())
			{
				const FName DebugName = ParameterView->GetName();
				GVoxelParameterGuidToDebugName.Add(ParameterPath, DebugName);
			}

			ParameterValue.Dependency = FVoxelDependency::Create(
				STATIC_FNAME("Parameter"),
				FName(FString::Printf(TEXT("GUID=%s Name=%s Type=%s"),
					*ParameterView->GetGuid().ToString(),
					*ParameterView->GetName().ToString(),
					*ParameterView->GetType().ToString())));
		}

		if (ParameterView->IsInlineGraph())
		{
			if (ParameterValue.Value.IsValid() &&
				ensure(ParameterValue.Value.Is<FVoxelInlineGraph>()) &&
				ensure(NewValue.Is<UVoxelGraphInterface>()) &&
				ParameterValue.Value.Get<FVoxelInlineGraph>().Data &&
				ParameterValue.Value.Get<FVoxelInlineGraph>().Data->WeakProvider == NewValue.Get<UVoxelGraphInterface>())
			{
				continue;
			}

			const TSharedRef<FVoxelInlineGraph> InlineGraph = MakeVoxelShared<FVoxelInlineGraph>();

			if (UVoxelGraphInterface* Graph = NewValue.Get<UVoxelGraphInterface>())
			{
				InlineGraph->Data = FVoxelInlineGraphData::Create(
					Graph,
					ParameterPath);
			}

			ParameterValue.Value = FVoxelRuntimePinValue::Make(InlineGraph);
		}
		else
		{
			if (ParameterValue.Value.IsValid() &&
				// TODO IsArray?
				ensure(!ParameterValue.Value.IsBuffer()) &&
				FVoxelPinType::MakeExposedValue(ParameterValue.Value, false) == NewValue)
			{
				continue;
			}

			ParameterValue.Value = FVoxelPinType::MakeRuntimeValue(ParameterView->GetType(), NewValue);
		}
		ensure(ParameterValue.Value.IsValid());

		ParameterValue.Dependency->Invalidate();
	}

	for (const TWeakInterfacePtr<IVoxelParameterProvider>& Provider : ParameterRootView->GetContext().VisitedProviders)
	{
		BindOnChanged(Provider);
	}
}

FVoxelRuntimePinValue FVoxelParameterValues::FindParameter_GameThread(
	const FVoxelPinType& Type,
	const FVoxelParameterPath& Path,
	const FVoxelQuery& Query)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	// Group all the invalidate calls
	const FVoxelDependencyInvalidationScope DependencyInvalidationScope;

	VOXEL_SCOPE_LOCK(CriticalSection);

	if (!PathToValue_RequiresLock.Contains(Path))
	{
		PathToValue_RequiresLock.Add(Path);
	}

	Update_GameThread_RequiresLock();

	const FParameterValue* Parameter = PathToValue_RequiresLock.Find(Path);
	if (!Parameter)
	{
		VOXEL_MESSAGE(Error, "{0}: INTERNAL ERROR: Missing parameter {1}",
			Query,
			Path.ToString());

		return FVoxelRuntimePinValue(Type);
	}

	if (!ensureVoxelSlow(Parameter->Value.GetType() == Type))
	{
		VOXEL_MESSAGE(Error, "{0}: INTERNAL ERROR: Invalid parameter type: {1} vs {2}",
			Query,
			Type.ToString(),
			Parameter->Value.GetType().ToString());

		return FVoxelRuntimePinValue(Type);
	}

	Query.GetDependencyTracker().AddDependency(Parameter->Dependency.ToSharedRef());
	return Parameter->Value;
}