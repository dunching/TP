// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelQuery.h"
#include "Editor/EditorEngine.h"
#include "VoxelChunkSpawner.generated.h"

class FVoxelRuntime;

DECLARE_UNIQUE_VOXEL_ID_EXPORT(VOXELGRAPHCORE_API, FVoxelChunkId);

extern VOXELGRAPHCORE_API int32 GVoxelChunkSpawnerMaxChunks;
extern VOXELGRAPHCORE_API float GVoxelChunkSpawnerCameraRefreshThreshold;

enum class EVoxelChunkAction
{
	Compute,
	SetTransitionMask,
	BeginDestroy,
	Destroy
};
struct FVoxelChunkAction
{
	EVoxelChunkAction Action = {};
	FVoxelChunkId ChunkId;

	uint8 TransitionMask = 0;
	TSharedPtr<const TVoxelUniqueFunction<void()>> OnComputeComplete;

	FVoxelChunkAction() = default;
	FVoxelChunkAction(const EVoxelChunkAction Action, const FVoxelChunkId ChunkId)
		: Action(Action)
		, ChunkId(ChunkId)
	{
	}
};

struct FVoxelChunkActionQueue
{
	TQueue<FVoxelChunkAction, EQueueMode::Mpsc> AsyncQueue;
	TQueue<FVoxelChunkAction, EQueueMode::Mpsc> GameQueue;

	void Enqueue(const FVoxelChunkAction& Action)
	{
		const auto WorldPtr = GEditor->GetEditorWorldContext().World();

		switch (Action.Action)
		{
		default: ensure(false);
		case EVoxelChunkAction::Compute:
		{
			const auto WorldPtr221 = GEditor->GetEditorWorldContext().World();
            const auto WorldPtr21 = GEditor->GetEditorWorldContext().World();
        }
		case EVoxelChunkAction::BeginDestroy:
		{
			const auto WorldPtr1 = GEditor->GetEditorWorldContext().World();

			AsyncQueue.Enqueue(Action);
		}
		break;
		case EVoxelChunkAction::SetTransitionMask:
		case EVoxelChunkAction::Destroy:
		{
			GameQueue.Enqueue(Action);
		}
		break;
		}
	}
};

struct VOXELGRAPHCORE_API FVoxelChunkRef
{
	const FVoxelChunkId ChunkId;
	const TSharedRef<FVoxelChunkActionQueue> Queue;

	FVoxelChunkRef(const FVoxelChunkId ChunkId, const TSharedRef<FVoxelChunkActionQueue>& Queue)
		: ChunkId(ChunkId)
		, Queue(Queue)
	{
	}
	~FVoxelChunkRef()
	{
		Queue->Enqueue(FVoxelChunkAction(EVoxelChunkAction::Destroy, ChunkId));
	}

	void BeginDestroy() const
	{
		Queue->Enqueue(FVoxelChunkAction(EVoxelChunkAction::BeginDestroy, ChunkId));
	}
	void Compute(TVoxelUniqueFunction<void()>&& OnComputeComplete = nullptr) const
	{
		FVoxelChunkAction Action(EVoxelChunkAction::Compute, ChunkId);
		if (OnComputeComplete)
		{
			Action.OnComputeComplete = MakeSharedCopy(MoveTemp(OnComputeComplete));
		}
		Queue->Enqueue(Action);
	}
	void SetTransitionMask(uint8 TransitionMask) const
	{
		FVoxelChunkAction Action(EVoxelChunkAction::SetTransitionMask, ChunkId);
		Action.TransitionMask = TransitionMask;
		Queue->Enqueue(Action);
	}
};

class VOXELGRAPHCORE_API FVoxelChunkSpawnerImpl : public TSharedFromThis<FVoxelChunkSpawnerImpl>
{
public:
	float VoxelSize = 0.f;
	TFunction<void(
		int32 LOD,
		int32 ChunkSize,
		const FVoxelBox& Bounds,
		TSharedPtr<FVoxelChunkRef>& OutChunkRef)> CreateChunkLambda;

	FVoxelChunkSpawnerImpl() = default;
	virtual ~FVoxelChunkSpawnerImpl() = default;

	TSharedRef<FVoxelChunkRef> CreateChunk(
		int32 LOD,
		int32 ChunkSize,
		const FVoxelBox& Bounds) const;

	virtual void Tick(FVoxelRuntime& Runtime) = 0;
	virtual void Refresh() = 0;
	virtual void Recreate() = 0;
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelChunkSpawner : public FVoxelVirtualStruct
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	virtual TSharedPtr<FVoxelChunkSpawnerImpl> MakeImpl() const
	{
		return nullptr;
	}
};