// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Buffer/VoxelFloatBuffers.h"

struct FVoxelPointIdBuffer;

struct VOXELGRAPHCORE_API FVoxelBufferUtilities
{
public:
	static void WritePositions2D(
		FVoxelFloatBufferStorage& OutPositionX,
		FVoxelFloatBufferStorage& OutPositionY,
		const FVector2f& Start,
		float Step,
		const FIntPoint& Size);

	static void WritePositions3D(
		FVoxelFloatBufferStorage& OutPositionX,
		FVoxelFloatBufferStorage& OutPositionY,
		FVoxelFloatBufferStorage& OutPositionZ,
		const FVector3f& Start,
		float Step,
		const FIntVector& Size);

	static void WritePositions_Unpacked(
		FVoxelFloatBufferStorage& OutPositionX,
		FVoxelFloatBufferStorage& OutPositionY,
		FVoxelFloatBufferStorage& OutPositionZ,
		const FVector3f& Start,
		float Step,
		const FIntVector& Size);

	static void WriteIntPositions_Unpacked(
		FVoxelInt32BufferStorage& OutPositionX,
		FVoxelInt32BufferStorage& OutPositionY,
		FVoxelInt32BufferStorage& OutPositionZ,
		const FIntVector& Start,
		int32 Step,
		const FIntVector& Size);

	template<typename Type, typename OutDataType>
	static void UnpackData(
		const TVoxelBufferStorage<Type>& InData,
		OutDataType& OutData,
		const FIntVector& Size)
	{
		VOXEL_FUNCTION_COUNTER();
		check(OutData.Num() == Size.X * Size.Y * Size.Z);

		if (InData.IsConstant())
		{
			FVoxelUtilities::SetAll(OutData, InData.GetConstant());
			return;
		}

		if (!ensure(InData.Num() == OutData.Num()))
		{
			return;
		}

		check(Size % 2 == 0);
		const FIntVector BlockSize = Size / 2;

		int32 ReadIndex = 0;
		for (int32 Z = 0; Z < BlockSize.Z; Z++)
		{
			for (int32 Y = 0; Y < BlockSize.Y; Y++)
			{
				int32 BaseWriteIndex =
					Size.X * 2 * Y +
					Size.X * Size.Y * 2 * Z;

				for (int32 X = 0; X < BlockSize.X; X++)
				{
					checkVoxelSlow(ReadIndex == 8 * FVoxelUtilities::Get3DIndex<int32>(Size / 2, X, Y, Z));
					checkVoxelSlow(BaseWriteIndex == FVoxelUtilities::Get3DIndex<int32>(Size, 2 * X, 2 * Y, 2 * Z));

#define LOOP(Block) \
					{ \
						const int32 WriteIndex = BaseWriteIndex + bool(Block & 0x1) + bool(Block & 0x2) * Size.X + bool(Block & 0x4) * Size.X * Size.Y; \
						checkVoxelSlow(WriteIndex == FVoxelUtilities::Get3DIndex<int32>(Size, \
							2 * X + bool(Block & 0x1), \
							2 * Y + bool(Block & 0x2), \
							2 * Z + bool(Block & 0x4))); \
						\
						OutData[WriteIndex] = InData.LoadFast(ReadIndex + Block); \
					}

					LOOP(0);
					LOOP(1);
					LOOP(2);
					LOOP(3);
					LOOP(4);
					LOOP(5);
					LOOP(6);
					LOOP(7);

#undef LOOP

					ReadIndex += 8;
					BaseWriteIndex += 2;
				}
			}
		}
	}

public:
	static FVoxelVectorBuffer ApplyTransform(const FVoxelVectorBuffer& Buffer, const FTransform& Transform);
	static FVoxelVectorBuffer ApplyInverseTransform(const FVoxelVectorBuffer& Buffer, const FTransform& Transform);

	static FVoxelVectorBuffer ApplyTransform(
		const FVoxelVectorBuffer& Buffer,
		const FVoxelVectorBuffer& Translation,
		const FVoxelQuaternionBuffer& Rotation,
		const FVoxelVectorBuffer& Scale);

	static FVoxelVectorBuffer ApplyTransform(const FVoxelVectorBuffer& Buffer, const FMatrix& Transform);
	static FVoxelFloatBuffer TransformDistance(const FVoxelFloatBuffer& Distance, const FMatrix& Transform);

public:
	static FVoxelFloatBuffer Min8(
		const FVoxelFloatBuffer& Buffer0,
		const FVoxelFloatBuffer& Buffer1,
		const FVoxelFloatBuffer& Buffer2,
		const FVoxelFloatBuffer& Buffer3,
		const FVoxelFloatBuffer& Buffer4,
		const FVoxelFloatBuffer& Buffer5,
		const FVoxelFloatBuffer& Buffer6,
		const FVoxelFloatBuffer& Buffer7);

	static FVoxelFloatBuffer Max8(
		const FVoxelFloatBuffer& Buffer0,
		const FVoxelFloatBuffer& Buffer1,
		const FVoxelFloatBuffer& Buffer2,
		const FVoxelFloatBuffer& Buffer3,
		const FVoxelFloatBuffer& Buffer4,
		const FVoxelFloatBuffer& Buffer5,
		const FVoxelFloatBuffer& Buffer6,
		const FVoxelFloatBuffer& Buffer7);

	static FVoxelVectorBuffer Min8(
		const FVoxelVectorBuffer& Buffer0,
		const FVoxelVectorBuffer& Buffer1,
		const FVoxelVectorBuffer& Buffer2,
		const FVoxelVectorBuffer& Buffer3,
		const FVoxelVectorBuffer& Buffer4,
		const FVoxelVectorBuffer& Buffer5,
		const FVoxelVectorBuffer& Buffer6,
		const FVoxelVectorBuffer& Buffer7);

	static FVoxelVectorBuffer Max8(
		const FVoxelVectorBuffer& Buffer0,
		const FVoxelVectorBuffer& Buffer1,
		const FVoxelVectorBuffer& Buffer2,
		const FVoxelVectorBuffer& Buffer3,
		const FVoxelVectorBuffer& Buffer4,
		const FVoxelVectorBuffer& Buffer5,
		const FVoxelVectorBuffer& Buffer6,
		const FVoxelVectorBuffer& Buffer7);

public:
	static FVoxelFloatBuffer IntToFloat(const FVoxelInt32Buffer& Buffer);
	static FVoxelDoubleBuffer IntToDouble(const FVoxelInt32Buffer& Buffer);

	static FVoxelDoubleBuffer FloatToDouble(const FVoxelFloatBuffer& Float);
	static FVoxelFloatBuffer DoubleToFloat(const FVoxelDoubleBuffer& Double);

	static FVoxelSeedBuffer PointIdToSeed(const FVoxelPointIdBuffer& Buffer);
	static FVoxelInt32Buffer BoolToInt32(const FVoxelBoolBuffer& Buffer);

public:
	static FVoxelFloatBuffer Add(const FVoxelFloatBuffer& A, const FVoxelFloatBuffer& B);
	static FVoxelFloatBuffer Multiply(const FVoxelFloatBuffer& A, const FVoxelFloatBuffer& B);

	static FVoxelVectorBuffer Add(const FVoxelVectorBuffer& A, const FVoxelVectorBuffer& B);
	static FVoxelVectorBuffer Multiply(const FVoxelVectorBuffer& A, const FVoxelVectorBuffer& B);

	static FVoxelQuaternionBuffer Combine(const FVoxelQuaternionBuffer& A, const FVoxelQuaternionBuffer& B);

public:
	static void MakePalette(
		const FVoxelTerminalBuffer& Buffer,
		FVoxelInt32Buffer& OutIndices,
		FVoxelTerminalBuffer& OutPalette);

public:
	static void Gather(
		FVoxelTerminalBuffer& OutBuffer,
		const FVoxelTerminalBuffer& Buffer,
		const FVoxelInt32Buffer& Indices);

	static TSharedRef<const FVoxelBuffer> Gather(
		const FVoxelBuffer& Buffer,
		const FVoxelInt32Buffer& Indices);

public:
	static void Replicate(
		FVoxelTerminalBuffer& OutBuffer,
		const FVoxelTerminalBuffer& Buffer,
		TConstVoxelArrayView<int32> Counts,
		int32 NewNum);

	static TSharedRef<const FVoxelBuffer> Replicate(
		const FVoxelBuffer& Buffer,
		TConstVoxelArrayView<int32> Counts,
		int32 NewNum);

public:
	static FVoxelFutureValue Query2D(
		const FVoxelComputeValue& ComputeValue,
		const FVoxelQuery& Query,
		const FVoxelPinType& BufferType);

private:
	static void ExpandQuery2D(
		FVoxelTerminalBuffer& OutBuffer,
		const FVoxelTerminalBuffer& Buffer,
		int32 Count);
};