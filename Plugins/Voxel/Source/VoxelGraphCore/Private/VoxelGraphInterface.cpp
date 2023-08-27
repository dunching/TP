// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelGraphInterface.h"

FString UVoxelGraphInterface::GetGraphName() const
{
	FString Name = GetName();
	Name.RemoveFromStart("VG_");
	Name.RemoveFromStart("VGI_");
	return FName::NameToDisplayString(Name, false);
}