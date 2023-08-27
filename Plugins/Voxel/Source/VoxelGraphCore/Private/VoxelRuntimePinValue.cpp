// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelRuntimePinValue.h"
#include "VoxelBuffer.h"

FVoxelRuntimePinValue::FVoxelRuntimePinValue(const FVoxelPinType& Type)
	: FVoxelRuntimePinValue()
{
	this->Type = Type;

	check(!Type.IsWildcard());
	check(!Type.GetInnerType().IsObject());

	if (Type.IsBuffer())
	{
		TSharedRef<FVoxelBuffer> Buffer = FVoxelBuffer::Make(Type.GetInnerType());
		SharedStructType = Buffer->GetStruct();
		SharedStruct = ReinterpretCastRef<TSharedRef<const FVoxelSharedStructOpaque>>(MoveTemp(Buffer));
	}
	else if (Type.IsStruct())
	{
		SharedStructType = Type.GetStruct();
		SharedStruct = MakeSharedStruct(SharedStructType);
	}
}

FVoxelRuntimePinValue FVoxelRuntimePinValue::MakeStruct(const FConstVoxelStructView Struct)
{
	FVoxelRuntimePinValue Result;
	Result.Type = FVoxelPinType::MakeStruct(Struct.GetStruct());
	Result.SharedStructType = Struct.GetStruct();
	Result.SharedStruct = Struct.MakeSharedCopy();
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelRuntimePinValue FVoxelRuntimePinValue::Make(
	const TSharedRef<const FVoxelBuffer>& Value,
	const FVoxelPinType& Type)
{
	checkVoxelSlow(Type.IsBuffer());

	FVoxelRuntimePinValue Result;
	Result.Type = Type;
	Result.SharedStructType = Value->GetStruct();
	Result.SharedStruct = ReinterpretCastRef<TSharedRef<const FVoxelSharedStructOpaque>>(Value);
	return Result;
}

FVoxelRuntimePinValue FVoxelRuntimePinValue::Make(
	const TSharedRef<FVoxelBuffer>& Value,
	const FVoxelPinType& Type)
{
	return Make(StaticCastSharedRef<const FVoxelBuffer>(Value), Type);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelRuntimePinValue::IsValidValue_Slow() const
{
	if (!Type.IsValid())
	{
		return false;
	}

	if (!Type.CanBeCastedTo<FVoxelBufferInterface>())
	{
		return true;
	}

	return Get<FVoxelBufferInterface>().IsValid_Slow();
}