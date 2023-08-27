// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMeshDataBase.h"
#include "VoxelAABBTree.h"

int64 FVoxelMeshDataBase::GetAllocatedSize() const
{
	int64 AllocatedSize = 0;
	AllocatedSize += PointIds.GetAllocatedSize();
	AllocatedSize += Transforms.GetAllocatedSize();
	AllocatedSize += CustomDatas_Transient.GetAllocatedSize();

	for (const TVoxelArray<float>& CustomData : CustomDatas_Transient)
	{
		AllocatedSize += CustomData.GetAllocatedSize();
	}

	return AllocatedSize;
}

TSharedRef<const FVoxelAABBTree> FVoxelMeshDataBase::GetTree() const
{
	VOXEL_SCOPE_LOCK(TreeCriticalSection);

	if (Tree_RequiresLock)
	{
		return Tree_RequiresLock.ToSharedRef();
	}

	VOXEL_FUNCTION_COUNTER();

	const FBox MeshBounds = Mesh.GetMeshInfo().MeshBox;
	if (!ensure(MeshBounds.IsValid))
	{
		return MakeVoxelShared<FVoxelAABBTree>();
	}
	const FVoxelBox VoxelMeshBounds(MeshBounds);

	TVoxelArray<FVoxelAABBTree::FElement> Elements;
	FVoxelUtilities::SetNumFast(Elements, Num());

	for (int32 Index = 0; Index < Num(); Index++)
	{
		FVoxelAABBTree::FElement& Element = Elements[Index];
		Element.Bounds = VoxelMeshBounds.TransformBy(Transforms[Index]);
		Element.Payload = Index;
	}

	const TSharedRef<FVoxelAABBTree> Tree = MakeVoxelShared<FVoxelAABBTree>();
	Tree->Initialize(MoveTemp(Elements));
	Tree_RequiresLock = Tree;

	return Tree;
}