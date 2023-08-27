// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelInvoker.h"
#include "VoxelActor.h"
#include "VoxelSettings.h"
#include "VoxelComponent.h"
#include "VoxelGraphExecutor.h"
#include "VoxelGraphNodeStatInterface.h"
#include "Material/VoxelMaterialDefinition.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

TSet<FObjectKey> GVoxelObjectsDestroyedByFrameRateLimit;

VOXEL_CONSOLE_COMMAND(
	RefreshAll,
	"voxel.RefreshAll",
	"")
{
	GVoxelOnClearNodeStats.Broadcast();

	GVoxelGraphExecutorManager->RecompileAll();

	ForEachObjectOfClass<UVoxelInvokerComponent>([&](UVoxelInvokerComponent* Component)
	{
		if (Component->IsTemplate())
		{
			return;
		}

		Component->UpdateInvoker();
	});

	ForEachObjectOfClass<AVoxelActor>([&](AVoxelActor* Actor)
	{
		if (Actor->IsRuntimeCreated() ||
			GVoxelObjectsDestroyedByFrameRateLimit.Contains(Actor))
		{
			Actor->QueueRecreate();
		}
	});

	ForEachObjectOfClass<UVoxelComponent>([&](UVoxelComponent* Component)
	{
		if (Component->IsRuntimeCreated() ||
			GVoxelObjectsDestroyedByFrameRateLimit.Contains(Component))
		{
			Component->QueueRecreate();
		}
	});

	ForEachObjectOfClass<UVoxelMaterialDefinition>([&](UVoxelMaterialDefinition* Definition)
	{
		Definition->QueueRebuildTextures();
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
// https://mveg.es/posts/fast-numerically-stable-moving-average/
struct FVoxelAverageDoubleBuffer
{
	explicit FVoxelAverageDoubleBuffer(const int32 ValuesCount)
		: WindowSize(1 << FMath::CeilLogTwo(ValuesCount))
	{
		ensure(FMath::IsPowerOfTwo(ValuesCount));
		FVoxelUtilities::SetNum(Values, WindowSize * 2);
	}

	void AddValue(const double NewValue)
	{
		const int32 PositionIndex = WindowSize - 1 + Position;
		Position = (Position + 1) % WindowSize;

		Values[PositionIndex] = NewValue;

		// Update parents
		for (int32 Index = PositionIndex; Index > 0; Index = GetParentIndex(Index))
		{
			const int32 ParentToUpdate = GetParentIndex(Index);
			Values[ParentToUpdate] = Values[GetLeftChildIndex(ParentToUpdate)] + Values[GetRightChildIndex(ParentToUpdate)];
		}
	}

	FORCEINLINE double GetAverageValue() const
	{
		return Values[0] / WindowSize;
	}

	FORCEINLINE int32 GetWindowSize() const
	{
		return WindowSize;
	}

private:
	static int32 GetParentIndex(const int32 ChildIndex)
	{
		check(ChildIndex > 0);
		return (ChildIndex - 1) / 2;
	}

	static int32 GetLeftChildIndex(const int32 ParentIndex)
	{
		return 2 * ParentIndex + 1;
	}

	static int32 GetRightChildIndex(const int32 ParentIndex)
	{
		return 2 * ParentIndex + 2;
	}

private:
	int32 WindowSize = 0;
	int32 Position = 0;
	TArray<double> Values;
};

class FVoxelSafetyTicker : public FVoxelTicker
{
public:
	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		if (GEditor->ShouldThrottleCPUUsage() ||
			GEditor->PlayWorld ||
			GIsPlayInEditorWorld)
		{
			// Don't check framerate when throttling or in PIE
			return;
		}

		const int32 FramesToAverage = FMath::Max(2, GetDefault<UVoxelSettings>()->FramesToAverage);
		if (FramesToAverage != Buffer.GetWindowSize())
		{
			Buffer = FVoxelAverageDoubleBuffer(FramesToAverage);
		}

		// Avoid outliers (typically, debugger breaking) causing a huge average
		const double SanitizedDeltaTime = FMath::Clamp(FApp::GetDeltaTime(), 0.001, 1);
		Buffer.AddValue(SanitizedDeltaTime);

		if (Buffer.GetAverageValue() < 1.f / GetDefault<UVoxelSettings>()->MinFPS)
		{
			bDestroyedRuntimes = false;
			return;
		}

		if (bDestroyedRuntimes)
		{
			return;
		}
		bDestroyedRuntimes = true;

		VOXEL_MESSAGE(Info, "Average framerate is below 8fps, destroying all voxel runtimes. Use Ctrl F5 to re-create them");

		ForEachObjectOfClass_Copy<AVoxelActor>([&](AVoxelActor* Actor)
		{
			if (!Actor->IsRuntimeCreated())
			{
				return;
			}

			if (!ensure(Actor->GetWorld()) ||
				!Actor->GetWorld()->IsEditorWorld())
			{
				return;
			}

			Actor->DestroyRuntime();
			GVoxelObjectsDestroyedByFrameRateLimit.Add(Actor);
		});

		ForEachObjectOfClass_Copy<UVoxelComponent>([&](UVoxelComponent* Component)
		{
			if (!Component->IsRuntimeCreated())
			{
				return;
			}

			if (!ensure(Component->GetWorld()) ||
				!Component->GetWorld()->IsEditorWorld())
			{
				return;
			}

			Component->DestroyRuntime();
			GVoxelObjectsDestroyedByFrameRateLimit.Add(Component);
		});
	}
	//~ End FVoxelTicker Interface

private:
	FVoxelAverageDoubleBuffer Buffer = FVoxelAverageDoubleBuffer(2);
	bool bDestroyedRuntimes = false;
};

VOXEL_RUN_ON_STARTUP_EDITOR(CreateVoxelSafetyTicker)
{
	if (!GEditor)
	{
		return;
	}

	new FVoxelSafetyTicker();
}
#endif