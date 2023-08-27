// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "Point/VoxelPointHandle.h"
#include "VoxelSpawnable.h"

TSharedPtr<FVoxelRuntime> FVoxelPointHandle::GetRuntime(FString* OutError) const
{
	return SpawnableRef.MergeSpawnableRef.GetRuntime(OutError);
}

bool FVoxelPointHandle::GetAttributes(
	TVoxelMap<FName, FVoxelPinValue>& OutAttributes,
	FString* OutError) const
{
	const TSharedPtr<FVoxelSpawnable> Spawnable = SpawnableRef.Resolve(OutError);
	if (!Spawnable.IsValid())
	{
		return false;
	}

	TVoxelArray<FVoxelPointId> PointIds;
	PointIds.Add(PointId);

	TVoxelMap<FName, TSharedPtr<const FVoxelBuffer>> Attributes;
	if (!Spawnable->GetPointAttributes(PointIds, Attributes))
	{
		if (OutError)
		{
			*OutError = "Failed to get attributes";
		}
		return false;
	}

	ensure(Attributes.Num() > 0);
	for (const auto& It : Attributes)
	{
		ensure(It.Value->Num() == 1);

		const FVoxelRuntimePinValue RuntimeValue = It.Value->GetGeneric(0);
		const FVoxelPinValue ExposedValue = FVoxelPinType::MakeExposedValue(RuntimeValue, false);
		if (!ensure(ExposedValue.IsValid()))
		{
			continue;
		}

		OutAttributes.Add(It.Key, ExposedValue);
	}

	return true;
}

bool FVoxelPointHandle::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!ensure(Map))
	{
		bOutSuccess = false;
		return true;
	}

	bOutSuccess = NetSerializeImpl(Ar, *Map);
	return true;
}

bool FVoxelPointHandle::NetSerializeImpl(FArchive& Ar, UPackageMap& Map)
{
	VOXEL_FUNCTION_COUNTER();

	if (Ar.IsSaving())
	{
		uint8 IsValidByte = IsValid();
		Ar.SerializeBits(&IsValidByte, 1);

		if (!IsValidByte)
		{
			return true;
		}

		if (!ensure(SpawnableRef.NetSerialize(Ar, Map)))
		{
			return false;
		}

		Ar << PointId.PointId;
		return true;
	}
	else if (Ar.IsLoading())
	{
		uint8 IsValidByte = 0;
		Ar.SerializeBits(&IsValidByte, 1);

		if (!IsValidByte)
		{
			*this = {};
			return true;
		}

		if (!ensure(SpawnableRef.NetSerialize(Ar, Map)))
		{
			return false;
		}

		Ar << PointId.PointId;

		return true;
	}
	else
	{
		ensure(false);
		return false;
	}
}