// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSpawnMeshNode.h"
#include "VoxelInvoker.h"
#include "VoxelRuntime.h"
#include "VoxelBufferUtilities.h"
#include "VoxelGraphNodeStatInterface.h"
#include "VoxelInstancedMeshComponent.h"
#include "VoxelHierarchicalMeshComponent.h"
#include "VoxelInstancedCollisionComponent.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELSPAWNER_API, int32, GVoxelMaxInstancesToRemoveAtOnce, 16,
	"voxel.MaxInstancesToRemoveAtOnce",
	"");

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_SpawnMesh, Out)
{
	const TSharedRef<FVoxelSpawnMeshSpawnable> Spawnable = MakeVoxelSpawnable<FVoxelSpawnMeshSpawnable>(Query, GetNodeRef());
	Spawnable->Initialize(Query, *this);
	return Spawnable;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnMeshSpawnable::Initialize(
	const FVoxelQuery& Query,
	const FVoxelNode_SpawnMesh& SpawnMesh)
{
	VOXEL_FUNCTION_COUNTER();

	PointsValue =
		SpawnMesh.GetNodeRuntime().MakeDynamicValueFactory(SpawnMesh.InPin)
		.Compute(
			Query.GetSharedContext(),
			MakeVoxelSpawnablePointQueryParameters(
				Query.GetSharedContext(),
				Query.GetSharedParameters(),
				GetNodeRef()));

	BodyInstanceValue =
		SpawnMesh.GetNodeRuntime().MakeDynamicValueFactory(SpawnMesh.BodyInstancePin)
		.Compute(Query.GetSharedContext(), Query.GetSharedParameters());

	FoliageSettingsValue =
		SpawnMesh.GetNodeRuntime().MakeDynamicValueFactory(SpawnMesh.FoliageSettingsPin)
		.Compute(Query.GetSharedContext(), Query.GetSharedParameters());

	StartCullDistanceValue =
		SpawnMesh.GetNodeRuntime().MakeDynamicValueFactory(SpawnMesh.StartCullDistancePin)
		.Compute(Query.GetSharedContext(), Query.GetSharedParameters());

	EndCullDistanceValue =
		SpawnMesh.GetNodeRuntime().MakeDynamicValueFactory(SpawnMesh.EndCullDistancePin)
		.Compute(Query.GetSharedContext(), Query.GetSharedParameters());

	CollisionInvokerChannelValue =
		SpawnMesh.GetNodeRuntime().MakeDynamicValueFactory(SpawnMesh.CollisionInvokerChannelPin)
		.Compute(Query.GetSharedContext(), Query.GetSharedParameters());

	PointsValue.OnChanged(MakeWeakPtrLambda(this, [this](const TSharedRef<const FVoxelPointSet>& NewValue)
	{
		UpdatePoints(NewValue);
	}));

	BodyInstanceValue.OnChanged_GameThread(MakeWeakPtrLambda(this, [this](const TSharedRef<const FBodyInstance>& NewValue)
	{
		if (BodyInstance_GameThread == NewValue)
		{
			return;
		}

		BodyInstance_GameThread = NewValue;

		if (!IsGameWorld())
		{
			return;
		}

		VOXEL_SCOPE_LOCK(CriticalSection);

		for (const auto& It : MeshToComponents_RequiresLock)
		{
			if (const UVoxelInstancedMeshComponent* Component = It.Value->InstancedMeshComponent.Get())
			{
				Component->UpdateCollision(*BodyInstance_GameThread);
			}
			if (const UVoxelHierarchicalMeshComponent* Component = It.Value->HierarchicalMeshComponent.Get())
			{
				check(CollisionInvokerChannel_GameThread.IsSet());
				Component->UpdateCollision(CollisionInvokerChannel_GameThread.GetValue(), *BodyInstance_GameThread);
			}
		}
	}));

	FoliageSettingsValue.OnChanged_GameThread(MakeWeakPtrLambda(this, [this](const TSharedRef<const FVoxelFoliageSettings>& NewValue)
	{
		if (FoliageSettings_GameThread == NewValue)
		{
			return;
		}

		FoliageSettings_GameThread = NewValue;

		VOXEL_SCOPE_LOCK(CriticalSection);

		for (const auto& It : MeshToComponents_RequiresLock)
		{
			if (UVoxelInstancedMeshComponent* Component = It.Value->InstancedMeshComponent.Get())
			{
				FoliageSettings_GameThread->ApplyToComponent(*Component);
			}
			if (UVoxelHierarchicalMeshComponent* Component = It.Value->HierarchicalMeshComponent.Get())
			{
				FoliageSettings_GameThread->ApplyToComponent(*Component);
			}
		}
	}));

	StartCullDistanceValue.OnChanged_GameThread(MakeWeakPtrLambda(this, [this](const float NewValue)
	{
		if (StartCullDistance_GameThread == NewValue)
		{
			return;
		}

		StartCullDistance_GameThread = NewValue;

		VOXEL_SCOPE_LOCK(CriticalSection);

		for (const auto& It : MeshToComponents_RequiresLock)
		{
			if (UVoxelInstancedMeshComponent* Component = It.Value->InstancedMeshComponent.Get())
			{
				Component->InstanceStartCullDistance = StartCullDistance_GameThread;
			}
			if (UVoxelHierarchicalMeshComponent* Component = It.Value->HierarchicalMeshComponent.Get())
			{
				Component->InstanceStartCullDistance = StartCullDistance_GameThread;
			}
		}
	}));

	EndCullDistanceValue.OnChanged_GameThread(MakeWeakPtrLambda(this, [this](const float NewValue)
	{
		if (EndCullDistance_GameThread == NewValue)
		{
			return;
		}

		EndCullDistance_GameThread = NewValue;

		VOXEL_SCOPE_LOCK(CriticalSection);

		for (const auto& It : MeshToComponents_RequiresLock)
		{
			if (UVoxelInstancedMeshComponent* Component = It.Value->InstancedMeshComponent.Get())
			{
				Component->InstanceEndCullDistance = EndCullDistance_GameThread;
			}
			if (UVoxelHierarchicalMeshComponent* Component = It.Value->HierarchicalMeshComponent.Get())
			{
				Component->InstanceEndCullDistance = EndCullDistance_GameThread;
			}
		}
	}));

	CollisionInvokerChannelValue.OnChanged_GameThread(MakeWeakPtrLambda(this, [this](const FName NewValue)
	{
		CollisionInvokerChannel_GameThread = NewValue;

		FlushPendingHierarchicalDatas_GameThread();

		VOXEL_SCOPE_LOCK(CriticalSection);
		for (const auto& It : MeshToComponents_RequiresLock)
		{
			if (const UVoxelHierarchicalMeshComponent* Component = It.Value->HierarchicalMeshComponent.Get())
			{
				Component->CollisionComponent->SetInvokerChannel(NewValue);
			}
		}
	}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnMeshSpawnable::Create_GameThread()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	FlushPendingHierarchicalDatas_GameThread();
}

void FVoxelSpawnMeshSpawnable::Destroy_GameThread()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);
	check(IsInGameThread());

	const TSharedPtr<FVoxelRuntime> Runtime = GetRuntime();
	if (!Runtime)
	{
		// Already destroyed
		return;
	}

	for (const auto& It : MeshToComponents_RequiresLock)
	{
		Runtime->DestroyComponent(It.Value->InstancedMeshComponent);
		Runtime->DestroyComponent(It.Value->HierarchicalMeshComponent);
	}
}

FVoxelOptionalBox FVoxelSpawnMeshSpawnable::GetBounds() const
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);
	ensure(IsInGameThread());

	FVoxelOptionalBox Result;
	for (const auto& It : MeshToComponents_RequiresLock)
	{
		const UVoxelHierarchicalMeshComponent* Component = It.Value->HierarchicalMeshComponent.Get();
		if (!Component)
		{
			continue;
		}

		const TSharedPtr<const FVoxelHierarchicalMeshData> MeshData = Component->GetMeshData();
		if (!ensure(MeshData))
		{
			continue;
		}

		ensure(MeshData->Bounds.IsValid());
		Result += MeshData->Bounds;
	}
	return Result;
}

bool FVoxelSpawnMeshSpawnable::GetPointAttributes(
	const TConstVoxelArrayView<FVoxelPointId> PointIds,
	TVoxelMap<FName, TSharedPtr<const FVoxelBuffer>>& OutAttributes) const
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	if (!Points_RequiresLock)
	{
		return false;
	}

	FVoxelInt32BufferStorage IndicesStorage;
	IndicesStorage.Allocate(PointIds.Num());

	for (int32 Index = 0; Index < PointIds.Num(); Index++)
	{
		if (const int32* IndexPtr = PointIdToIndex_RequiresLock.Find(PointIds[Index]))
		{
			IndicesStorage[Index] = *IndexPtr;
		}
		else
		{
			IndicesStorage[Index] = -1;
		}
	}

	const FVoxelInt32Buffer Indices = FVoxelInt32Buffer::Make(IndicesStorage);
	for (const auto& It : Points_RequiresLock->GetAttributes())
	{
		OutAttributes.Add(It.Key, FVoxelBufferUtilities::Gather(*It.Value, Indices));
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnMeshSpawnable::UpdatePoints(const TSharedRef<const FVoxelPointSet>& NewPoints)
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelSpawnMeshSpawnable::UpdatePoints Num=%d", NewPoints->Num());
	ensure(!IsInGameThread());
	FVoxelNodeStatScope StatScope(*this, NewPoints->Num());

	FindVoxelPointSetAttributeVoid(*NewPoints, FVoxelPointAttributes::Id, FVoxelPointIdBuffer, NewIds);
	FindVoxelPointSetAttributeVoid(*NewPoints, FVoxelPointAttributes::Mesh, FVoxelStaticMeshBuffer, NewMeshes);
	FindVoxelPointSetAttributeVoid(*NewPoints, FVoxelPointAttributes::Position, FVoxelVectorBuffer, NewPositions);
	FindVoxelPointSetOptionalAttribute(*NewPoints, FVoxelPointAttributes::Rotation, FVoxelQuaternionBuffer, NewRotations, FQuat::Identity);
	FindVoxelPointSetOptionalAttribute(*NewPoints, FVoxelPointAttributes::Scale, FVoxelVectorBuffer, NewScales, FVector::OneVector);
	const TVoxelArray<FVoxelFloatBuffer> NewCustomDatas = NewPoints->FindCustomDatas(GetNodeRef());

	TVoxelAddOnlyMap<FVoxelPointId, int32> NewPointIdToIndex;
	{
		VOXEL_SCOPE_COUNTER("PointIdToIndex");

		NewPointIdToIndex.Reserve(NewPoints->Num());

		for (int32 Index = 0; Index < NewPoints->Num(); Index++)
		{
			const FVoxelPointId PointId = NewIds[Index];
			if (NewPointIdToIndex.Contains(PointId))
			{
				VOXEL_MESSAGE(Error, "{0}: PointIds duplicates!", this);
				continue;
			}

			NewPointIdToIndex.Add_CheckNew_NoRehash(PointId, Index);
		}
	}

	VOXEL_SCOPE_LOCK(CriticalSection);

	ON_SCOPE_EXIT
	{
		PointIdToIndex_RequiresLock = MoveTemp(NewPointIdToIndex);
		Points_RequiresLock = NewPoints;
	};

	if (!Points_RequiresLock)
	{
		UpdatePoints_Hierarchical_AssumeLocked(NewPoints);
		return;
	}

	FindVoxelPointSetAttributeVoid(*Points_RequiresLock, FVoxelPointAttributes::Id, FVoxelPointIdBuffer, OldIds);
	FindVoxelPointSetAttributeVoid(*Points_RequiresLock, FVoxelPointAttributes::Mesh, FVoxelStaticMeshBuffer, OldMeshes);
	FindVoxelPointSetAttributeVoid(*Points_RequiresLock, FVoxelPointAttributes::Position, FVoxelVectorBuffer, OldPositions);
	FindVoxelPointSetOptionalAttribute(*Points_RequiresLock, FVoxelPointAttributes::Rotation, FVoxelQuaternionBuffer, OldRotations, FQuat::Identity);
	FindVoxelPointSetOptionalAttribute(*Points_RequiresLock, FVoxelPointAttributes::Scale, FVoxelVectorBuffer, OldScales, FVector::OneVector);
	const TVoxelArray<FVoxelFloatBuffer> OldCustomDatas = Points_RequiresLock->FindCustomDatas(GetNodeRef());

	if (NewCustomDatas.Num() != OldCustomDatas.Num())
	{
		UpdatePoints_Hierarchical_AssumeLocked(NewPoints);
		return;
	}
	const int32 NumCustomDatas = NewCustomDatas.Num();

	TVoxelArray<int32> NewToOldIndex;
	FVoxelUtilities::SetNumFast(NewToOldIndex, NewPointIdToIndex.Num());
	{
		VOXEL_SCOPE_COUNTER("NewToOldIndex");

		for (const auto& It : NewPointIdToIndex)
		{
			if (const int32* IndexPtr = PointIdToIndex_RequiresLock.Find(It.Key))
			{
				NewToOldIndex[It.Value] = *IndexPtr;
			}
			else
			{
				NewToOldIndex[It.Value] = -1;
			}
		}
	}

	TVoxelChunkedArray<FVoxelPointId> PointIdsToRemove;
	TVoxelAddOnlyMap<FVoxelStaticMesh, TSharedPtr<FVoxelInstancedMeshData>> MeshToMeshData;
	{
		VOXEL_SCOPE_COUNTER("MeshToMeshData")

		for (int32 NewIndex = 0; NewIndex < NewToOldIndex.Num(); NewIndex++)
		{
			const FVoxelStaticMesh Mesh = NewMeshes[NewIndex];
			const FVector3f Position = NewPositions[NewIndex];
			const FQuat4f Rotation = NewRotations[NewIndex];
			const FVector3f Scale = NewScales[NewIndex];

			const int32 OldIndex = NewToOldIndex[NewIndex];
			if (OldIndex != -1)
			{
				checkVoxelSlow(OldIds[OldIndex] == NewIds[NewIndex]);

				const FVoxelStaticMesh OldMesh = OldMeshes[OldIndex];
				const FVector3f OldPosition = OldPositions[OldIndex];
				const FQuat4f OldRotation = OldRotations[OldIndex];
				const FVector3f OldScale = OldScales[OldIndex];

				if (OldMesh != Mesh)
				{
					goto DifferentData;
				}

				if (OldPosition != Position ||
					OldRotation != Rotation ||
					OldScale != Scale)
				{
					goto DifferentData;
				}

				for (int32 Index = 0; Index < NumCustomDatas; Index++)
				{
					if (OldCustomDatas[Index][OldIndex] != NewCustomDatas[Index][NewIndex])
					{
						goto DifferentData;
					}
				}

				continue;

			DifferentData:
				PointIdsToRemove.Add(OldIds[OldIndex]);

				if (PointIdsToRemove.Num() > GVoxelMaxInstancesToRemoveAtOnce)
				{
					UpdatePoints_Hierarchical_AssumeLocked(NewPoints);
					return;
				}
			}

			const FVoxelPointId PointId = NewIds[NewIndex];

			TSharedPtr<FVoxelInstancedMeshData>& MeshData = MeshToMeshData.FindOrAdd(Mesh);
			if (!MeshData)
			{
				MeshData = MakeVoxelShared<FVoxelInstancedMeshData>(Mesh, GetSpawnableRef());
				MeshData->NumCustomDatas = NumCustomDatas;
				MeshData->CustomDatas_Transient.SetNum(NumCustomDatas);
			}

			MeshData->PointIds.Add(PointId);
			MeshData->Transforms.Add(FTransform3f(Rotation, Position, Scale));

			for (int32 Index = 0; Index < NumCustomDatas; Index++)
			{
				MeshData->CustomDatas_Transient[Index].Add(NewCustomDatas[Index][NewIndex]);
			}
		}
	}

	for (auto& It : MeshToMeshData)
	{
		It.Value->Build();
	}

	TVoxelAddOnlyMap<FVoxelStaticMesh, TVoxelArray<int32>> MeshToInstancedInstancesToRemove;
	TVoxelAddOnlyMap<FVoxelStaticMesh, TVoxelArray<int32>> MeshToHierarchicalInstancesToRemove;
	{
		VOXEL_SCOPE_COUNTER("PointIdsToRemove");

		for (const auto& It : PointIdToIndex_RequiresLock)
		{
			if (!NewPointIdToIndex.Contains(It.Key))
			{
				PointIdsToRemove.Add(It.Key);
			}
		}

		for (const FVoxelPointId PointId : PointIdsToRemove)
		{
			const int32* OldIndexPtr = PointIdToIndex_RequiresLock.Find(PointId);
			if (!ensure(OldIndexPtr))
			{
				continue;
			}

			const FVoxelStaticMesh Mesh = OldMeshes[*OldIndexPtr];
			const TSharedPtr<FComponents>* ComponentsPtr = MeshToComponents_RequiresLock.Find(Mesh);
			if (!ensure(ComponentsPtr))
			{
				continue;
			}

			FIndexInfo* IndexInfo = (**ComponentsPtr).PointIdToIndexInfo.Find(PointId);
			if (!ensure(IndexInfo) ||
				!ensure(IndexInfo->bIsValid))
			{
				continue;
			}

			if (!IndexInfo->bIsHierarchical)
			{
				MeshToInstancedInstancesToRemove.FindOrAdd(Mesh).Add(IndexInfo->Index);
			}
			else
			{
				MeshToHierarchicalInstancesToRemove.FindOrAdd(Mesh).Add(IndexInfo->Index);
			}

			IndexInfo->Raw = -1;
			IndexInfo->bIsValid = false;
		}
	}

	for (const auto& It : MeshToMeshData)
	{
		VOXEL_SCOPE_COUNTER("PointIdToIndexInfo");

		TSharedPtr<FComponents>& ComponentsPtr = MeshToComponents_RequiresLock.FindOrAdd(It.Key);
		if (!ComponentsPtr)
		{
			ComponentsPtr = MakeVoxelShared<FComponents>();
		}

		FComponents& Components = *ComponentsPtr;
		FVoxelInstancedMeshData& MeshData = *It.Value;
		TVoxelArray<int32>& InstancedInstancesToRemove = MeshToInstancedInstancesToRemove.FindOrAdd(It.Key);

		MeshData.InstanceIndices.Reserve(MeshData.Num());
		Components.PointIdToIndexInfo.Reserve(Components.PointIdToIndexInfo.Num() + MeshData.Num());

		for (const FVoxelPointId PointId : MeshData.PointIds)
		{
			FIndexInfo& IndexInfo = Components.PointIdToIndexInfo.FindOrAdd(PointId);
			if (!ensureVoxelSlow(!IndexInfo.bIsValid))
			{
				continue;
			}

			int32 Index;
			if (InstancedInstancesToRemove.Num() > 0)
			{
				// Steal an index we're going to remove
				Index = InstancedInstancesToRemove.Pop(false);
			}
			else if (Components.FreeInstancedIndices.Num() > 0)
			{
				Index = Components.FreeInstancedIndices.Pop(false);
			}
			else
			{
				MeshData.NumNewInstances++;
				Index = Components.NumInstancedInstances++;
			}
			MeshData.InstanceIndices.Add(Index);

			IndexInfo.bIsValid = true;
			IndexInfo.bIsHierarchical = false;
			IndexInfo.Index = Index;
		}

		Components.FreeInstancedIndices.Append(InstancedInstancesToRemove);
	}

	FVoxelUtilities::RunOnGameThread(MakeWeakPtrLambda(this, [
		this,
		MeshToMeshData = MoveTemp(MeshToMeshData),
		MeshToInstancedInstancesToRemove = MoveTemp(MeshToInstancedInstancesToRemove),
		MeshToHierarchicalInstancesToRemove = MoveTemp(MeshToHierarchicalInstancesToRemove)]
	{
		VOXEL_SCOPE_COUNTER("UpdatePoints_GameThread");
		VOXEL_SCOPE_LOCK(CriticalSection);

		if (IsDestroyed())
		{
			return;
		}

		const TSharedPtr<FVoxelRuntime> Runtime = GetRuntime();
		if (!ensure(Runtime))
		{
			return;
		}

		// Make sure to process instances to remove BEFORE setting new mesh data,
		// as it might reuse the same indices

		for (const auto& It : MeshToInstancedInstancesToRemove)
		{
			if (It.Value.Num() == 0)
			{
				// Indices were reused or we added it through the FindOrAdd above
				continue;
			}

			const TSharedPtr<FComponents> Components = MeshToComponents_RequiresLock.FindRef(It.Key);
			if (!ensure(Components))
			{
				continue;
			}

			UVoxelInstancedMeshComponent* Component = Components->InstancedMeshComponent.Get();
			if (!ensure(Component))
			{
				continue;
			}

			Component->RemoveInstancesFast(It.Value);
		}

		for (const auto& It : MeshToHierarchicalInstancesToRemove)
		{
			const TSharedPtr<FComponents> Components = MeshToComponents_RequiresLock.FindRef(It.Key);
			if (!ensure(Components))
			{
				continue;
			}

			UVoxelHierarchicalMeshComponent* Component = Components->HierarchicalMeshComponent.Get();
			if (!ensure(Component))
			{
				continue;
			}

			Component->RemoveInstancesFast(It.Value);
		}

		for (const auto& It : MeshToMeshData)
		{
			const TSharedPtr<FComponents> Components = MeshToComponents_RequiresLock.FindRef(It.Key);
			if (!ensure(Components))
			{
				continue;
			}

			UVoxelInstancedMeshComponent* Component = Components->InstancedMeshComponent.Get();
			if (!Component)
			{
				Component = Runtime->CreateComponent<UVoxelInstancedMeshComponent>();
				Components->InstancedMeshComponent = Component;
			}
			if (!ensure(Component))
			{
				continue;
			}

			if (!Component->HasMeshData())
			{
				Component->SetMeshData(It.Value.ToSharedRef());

				if (BodyInstance_GameThread &&
					IsGameWorld())
				{
					Component->UpdateCollision(*BodyInstance_GameThread);
				}

				if (FoliageSettings_GameThread)
				{
					FoliageSettings_GameThread->ApplyToComponent(*Component);
				}
			}
			else
			{
				Component->AddMeshData(It.Value.ToSharedRef());
			}

			Component->InstanceStartCullDistance = StartCullDistance_GameThread;
			Component->InstanceEndCullDistance = EndCullDistance_GameThread;
		}
	}));
}

void FVoxelSpawnMeshSpawnable::UpdatePoints_Hierarchical_AssumeLocked(const TSharedRef<const FVoxelPointSet>& Points)
{
	VOXEL_FUNCTION_COUNTER();
	check(CriticalSection.IsLocked());

	// Clear existing data
	for (auto& It : MeshToComponents_RequiresLock)
	{
		It.Value->PointIdToIndexInfo.Reset();
		It.Value->FreeInstancedIndices.Reset();
		It.Value->NumInstancedInstances = 0;
	}

	if (Points->Num() == 0)
	{
		FVoxelUtilities::RunOnGameThread(MakeWeakPtrLambda(this, [this]
		{
			if (IsCreated() &&
				!IsDestroyed())
			{
				SetHierarchicalDatas_GameThread({});
			}
		}));
		return;
	}

	FindVoxelPointSetAttributeVoid(*Points, FVoxelPointAttributes::Mesh, FVoxelStaticMeshBuffer, Meshes);

	FVoxelInt32Buffer MeshIndices;
	FVoxelStaticMeshBuffer MeshPalette;
	FVoxelBufferUtilities::MakePalette(Meshes, MeshIndices, MeshPalette);

	for (const FVoxelStaticMesh& Mesh : MeshPalette)
	{
		if (!Mesh.StaticMesh.IsValid())
		{
			VOXEL_MESSAGE(Error, "{0}: Mesh is null", this);
		}
	}

	TVoxelArray<int32> MeshIndexToNumInstances;
	MeshIndexToNumInstances.SetNum(MeshPalette.Num());
	{
		VOXEL_SCOPE_COUNTER("MeshIndexToNumInstances");

		for (int32 Index = 0; Index < Points->Num(); Index++)
		{
			MeshIndexToNumInstances[MeshIndices[Index]]++;
		}
	}

	TVoxelArray<TSharedPtr<FVoxelHierarchicalMeshData>> HierarchicalMeshDatas;
	for (const FVoxelStaticMesh& Mesh : MeshPalette)
	{
		HierarchicalMeshDatas.Add(MakeVoxelShared<FVoxelHierarchicalMeshData>(Mesh, GetSpawnableRef()));
	}

	{
		VOXEL_SCOPE_COUNTER("PointIds & Transforms");

		FindVoxelPointSetAttributeVoid(*Points, FVoxelPointAttributes::Id, FVoxelPointIdBuffer, Ids);

		for (int32 MeshIndex = 0; MeshIndex < HierarchicalMeshDatas.Num(); MeshIndex++)
		{
			FVoxelHierarchicalMeshData& MeshData = *HierarchicalMeshDatas[MeshIndex];
			const int32 NumInstances = MeshIndexToNumInstances[MeshIndex];

			MeshData.PointIds.Reserve(NumInstances);
			MeshData.Transforms.Reserve(NumInstances);
		}

		FindVoxelPointSetAttributeVoid(*Points, FVoxelPointAttributes::Position, FVoxelVectorBuffer, Positions);
		FindVoxelPointSetOptionalAttribute(*Points, FVoxelPointAttributes::Rotation, FVoxelQuaternionBuffer, Rotations, FQuat::Identity);
		FindVoxelPointSetOptionalAttribute(*Points, FVoxelPointAttributes::Scale, FVoxelVectorBuffer, Scales, FVector::OneVector);

		for (int32 Index = 0; Index < Points->Num(); Index++)
		{
			FVoxelHierarchicalMeshData& MeshData = *HierarchicalMeshDatas[MeshIndices[Index]].Get();
			MeshData.PointIds.Add_NoGrow(Ids[Index]);
			MeshData.Transforms.Add_NoGrow(FTransform3f(Rotations[Index], Positions[Index], Scales[Index]));
		}
	}

	{
		VOXEL_SCOPE_COUNTER("CustomDatas");

		const TVoxelArray<FVoxelFloatBuffer> CustomDatas = Points->FindCustomDatas(GetNodeRef());

		for (int32 MeshIndex = 0; MeshIndex < HierarchicalMeshDatas.Num(); MeshIndex++)
		{
			FVoxelHierarchicalMeshData& MeshData = *HierarchicalMeshDatas[MeshIndex];
			const int32 NumInstances = MeshIndexToNumInstances[MeshIndex];

			MeshData.NumCustomDatas = CustomDatas.Num();
			MeshData.CustomDatas_Transient.SetNum(CustomDatas.Num());
			for (TVoxelArray<float>& CustomData : MeshData.CustomDatas_Transient)
			{
				CustomData.Reserve(NumInstances);
			}
		}

		for (int32 CustomDataIndex = 0; CustomDataIndex < CustomDatas.Num(); CustomDataIndex++)
		{
			const FVoxelFloatBuffer& CustomDataBuffer = CustomDatas[CustomDataIndex];

			for (int32 Index = 0; Index < Points->Num(); Index++)
			{
				FVoxelHierarchicalMeshData& MeshData = *HierarchicalMeshDatas[MeshIndices[Index]].Get();
				MeshData.CustomDatas_Transient[CustomDataIndex].Add_NoGrow(CustomDataBuffer[Index]);
			}
		}
	}

	for (const TSharedPtr<FVoxelHierarchicalMeshData>& HierarchicalMeshData : HierarchicalMeshDatas)
	{
		HierarchicalMeshData->Build();
	}

	for (const TSharedPtr<FVoxelHierarchicalMeshData>& HierarchicalMeshData : HierarchicalMeshDatas)
	{
		VOXEL_SCOPE_COUNTER("PointIdToIndexInfo");

		TSharedPtr<FComponents>& ComponentsPtr = MeshToComponents_RequiresLock.FindOrAdd(HierarchicalMeshData->Mesh);
		if (!ComponentsPtr)
		{
			ComponentsPtr = MakeVoxelShared<FComponents>();
		}
		FComponents& Components = *ComponentsPtr;

		ensure(Components.PointIdToIndexInfo.Num() == 0);
		Components.PointIdToIndexInfo.Reserve(HierarchicalMeshData->Num());

		const TVoxelArray<FVoxelPointId>& PointIds = HierarchicalMeshData->PointIds;
		for (int32 Index = 0; Index < PointIds.Num(); Index++)
		{
			const FVoxelPointId PointId = PointIds[Index];
			FIndexInfo& IndexInfo = Components.PointIdToIndexInfo.FindOrAdd(PointId);
			if (!ensureVoxelSlow(!IndexInfo.bIsValid))
			{
				continue;
			}

			IndexInfo.bIsValid = true;
			IndexInfo.bIsHierarchical = true;
			IndexInfo.Index = Index;
		}
	}

	FVoxelUtilities::RunOnGameThread(MakeWeakPtrLambda(this, [this, HierarchicalMeshDatas]
	{
		if (IsDestroyed())
		{
			return;
		}

		SetHierarchicalDatas_GameThread(HierarchicalMeshDatas);
	}));
}

void FVoxelSpawnMeshSpawnable::SetHierarchicalDatas_GameThread(
	const TVoxelArray<TSharedPtr<FVoxelHierarchicalMeshData>>& NewHierarchicalMeshDatas)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	if (!IsCreated() ||
		!CollisionInvokerChannel_GameThread.IsSet())
	{
		ensure(PendingHierarchicalMeshDatas_GameThread.Num() == 0);
		PendingHierarchicalMeshDatas_GameThread = NewHierarchicalMeshDatas;
		return;
	}

	const TSharedPtr<FVoxelRuntime> Runtime = GetRuntime();
	if (!ensure(Runtime))
	{
		return;
	}

	VOXEL_SCOPE_LOCK(CriticalSection);

	// Clear existing meshes
	for (const auto& It : MeshToComponents_RequiresLock)
	{
		ensure(It.Value->FreeInstancedIndices.Num() == 0);
		ensure(It.Value->NumInstancedInstances == 0);

		if (UVoxelInstancedMeshComponent* Component = It.Value->InstancedMeshComponent.Get())
		{
			Component->ClearInstances();
		}
		if (UVoxelHierarchicalMeshComponent* Component = It.Value->HierarchicalMeshComponent.Get())
		{
			Component->ClearInstances();
		}
	}

	for (const TSharedPtr<FVoxelHierarchicalMeshData>& HierarchicalData : NewHierarchicalMeshDatas)
	{
		const FVoxelStaticMesh Mesh = HierarchicalData->Mesh;
		if (!Mesh.StaticMesh.IsValid())
		{
			continue;
		}

		const TSharedPtr<FComponents> Components = MeshToComponents_RequiresLock.FindRef(Mesh);
		if (!ensure(Components))
		{
			continue;
		}

		UVoxelHierarchicalMeshComponent* Component = Components->HierarchicalMeshComponent.Get();
		if (!Component)
		{
			Component = Runtime->CreateComponent<UVoxelHierarchicalMeshComponent>();
			Components->HierarchicalMeshComponent = Component;
		}
		if (!ensure(Component))
		{
			continue;
		}

		Component->SetMeshData(HierarchicalData.ToSharedRef());

		if (BodyInstance_GameThread &&
			IsGameWorld())
		{
			Component->UpdateCollision(CollisionInvokerChannel_GameThread.GetValue(), *BodyInstance_GameThread);
		}

		if (FoliageSettings_GameThread)
		{
			FoliageSettings_GameThread->ApplyToComponent(*Component);
		}

		Component->InstanceStartCullDistance = StartCullDistance_GameThread;
		Component->InstanceEndCullDistance = EndCullDistance_GameThread;
	}
}

void FVoxelSpawnMeshSpawnable::FlushPendingHierarchicalDatas_GameThread()
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	if (PendingHierarchicalMeshDatas_GameThread.Num() == 0)
	{
		return;
	}

	const TVoxelArray<TSharedPtr<FVoxelHierarchicalMeshData>> HierarchicalMeshDatas = MoveTemp(PendingHierarchicalMeshDatas_GameThread);
	ensure(PendingHierarchicalMeshDatas_GameThread.Num() == 0);

	SetHierarchicalDatas_GameThread(HierarchicalMeshDatas);
}