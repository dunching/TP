// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelBufferUtilities.h"
#include "Point/VoxelPointId.h"
#include "VoxelPositionQueryParameter.h"
#include "VoxelBufferUtilitiesImpl.ispc.generated.h"

void FVoxelBufferUtilities::WritePositions2D(
	FVoxelFloatBufferStorage& OutPositionX,
	FVoxelFloatBufferStorage& OutPositionY,
	const FVector2f& Start,
	const float Step,
	const FIntPoint& Size)
{
	VOXEL_FUNCTION_COUNTER_NUM(Size.X * Size.Y, 1024);

	ensure(Size % 2 == 0);
	ensure(OutPositionX.Num() == Size.X * Size.Y);
	ensure(OutPositionY.Num() == Size.X * Size.Y);

	if (!ensure(Size.X < 256) ||
		!ensure(Size.Y < 256))
	{
		return;
	}

	const FIntPoint BlockSize = Size / 2;

	ForeachVoxelBufferChunk(Size.X * Size.Y, [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_WritePositions2D(
			OutPositionX.GetData(Iterator),
			OutPositionY.GetData(Iterator),
			Iterator.GetIndex(),
			Iterator.Num(),
			BlockSize.X,
			Start.X,
			Start.Y,
			Step);
	});
}

void FVoxelBufferUtilities::WritePositions3D(
	FVoxelFloatBufferStorage& OutPositionX,
	FVoxelFloatBufferStorage& OutPositionY,
	FVoxelFloatBufferStorage& OutPositionZ,
	const FVector3f& Start,
	const float Step,
	const FIntVector& Size)
{
	VOXEL_FUNCTION_COUNTER_NUM(Size.X * Size.Y * Size.Z, 1024);

	ensure(Size % 2 == 0);
	ensure(OutPositionX.Num() == Size.X * Size.Y * Size.Z);
	ensure(OutPositionY.Num() == Size.X * Size.Y * Size.Z);
	ensure(OutPositionZ.Num() == Size.X * Size.Y * Size.Z);

	if (!ensure(Size.X < 256) ||
		!ensure(Size.Y < 256) ||
		!ensure(Size.Z < 256))
	{
		return;
	}

	const FIntVector BlockSize = Size / 2;

	ForeachVoxelBufferChunk(Size.X * Size.Y * Size.Z, [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_WritePositions3D(
			OutPositionX.GetData(Iterator),
			OutPositionY.GetData(Iterator),
			OutPositionZ.GetData(Iterator),
			Iterator.GetIndex(),
			Iterator.Num(),
			BlockSize.X,
			BlockSize.Y,
			Start.X,
			Start.Y,
			Start.Z,
			Step);
	});
}

void FVoxelBufferUtilities::WritePositions_Unpacked(
	FVoxelFloatBufferStorage& OutPositionX,
	FVoxelFloatBufferStorage& OutPositionY,
	FVoxelFloatBufferStorage& OutPositionZ,
	const FVector3f& Start,
	const float Step,
	const FIntVector& Size)
{
	VOXEL_FUNCTION_COUNTER_NUM(Size.X * Size.Y * Size.Z, 1024);

	ensure(Size % 2 == 0);
	ensure(OutPositionX.Num() == Size.X * Size.Y * Size.Z);
	ensure(OutPositionY.Num() == Size.X * Size.Y * Size.Z);
	ensure(OutPositionZ.Num() == Size.X * Size.Y * Size.Z);

	int32 Index = 0;
	for (int32 Z = 0; Z < Size.Z; Z++)
	{
		for (int32 Y = 0; Y < Size.Y; Y++)
		{
			for (int32 X = 0; X < Size.X; X++)
			{
				checkVoxelSlow(Index == FVoxelUtilities::Get3DIndex<int32>(Size, X, Y, Z));

				OutPositionX[Index] = Start.X + X * Step;
				OutPositionY[Index] = Start.Y + Y * Step;
				OutPositionZ[Index] = Start.Z + Z * Step;

				Index++;
			}
		}
	}
}

void FVoxelBufferUtilities::WriteIntPositions_Unpacked(
	FVoxelInt32BufferStorage& OutPositionX,
	FVoxelInt32BufferStorage& OutPositionY,
	FVoxelInt32BufferStorage& OutPositionZ,
	const FIntVector& Start,
	const int32 Step,
	const FIntVector& Size)
{
	VOXEL_FUNCTION_COUNTER_NUM(Size.X * Size.Y * Size.Z, 1024);

	ensure(Size % 2 == 0);
	ensure(OutPositionX.Num() == Size.X * Size.Y * Size.Z);
	ensure(OutPositionY.Num() == Size.X * Size.Y * Size.Z);
	ensure(OutPositionZ.Num() == Size.X * Size.Y * Size.Z);

	int32 Index = 0;
	for (int32 Z = 0; Z < Size.Z; Z++)
	{
		for (int32 Y = 0; Y < Size.Y; Y++)
		{
			for (int32 X = 0; X < Size.X; X++)
			{
				checkVoxelSlow(Index == FVoxelUtilities::Get3DIndex<int32>(Size, X, Y, Z));

				OutPositionX[Index] = Start.X + X * Step;
				OutPositionY[Index] = Start.Y + Y * Step;
				OutPositionZ[Index] = Start.Z + Z * Step;

				Index++;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelVectorBuffer FVoxelBufferUtilities::ApplyTransform(const FVoxelVectorBuffer& Buffer, const FTransform& Transform)
{
	if (Transform.Equals(FTransform::Identity))
	{
		return Buffer;
	}

	VOXEL_FUNCTION_COUNTER_NUM(Buffer.Num(), 1024);

	FVoxelFloatBufferStorage ResultX; ResultX.Allocate(Buffer.Num());
	FVoxelFloatBufferStorage ResultY; ResultY.Allocate(Buffer.Num());
	FVoxelFloatBufferStorage ResultZ; ResultZ.Allocate(Buffer.Num());

	ForeachVoxelBufferChunk(Buffer.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_ApplyTransform(
			Buffer.X.GetData(Iterator), Buffer.X.IsConstant(),
			Buffer.Y.GetData(Iterator), Buffer.Y.IsConstant(),
			Buffer.Z.GetData(Iterator), Buffer.Z.IsConstant(),
			Iterator.Num(),
			GetISPCValue(FVector3f(Transform.GetTranslation())),
			GetISPCValue(FVector4f(
				Transform.GetRotation().X,
				Transform.GetRotation().Y,
				Transform.GetRotation().Z,
				Transform.GetRotation().W)),
			GetISPCValue(FVector3f(Transform.GetScale3D())),
			ResultX.GetData(Iterator),
			ResultY.GetData(Iterator),
			ResultZ.GetData(Iterator));
	});

	FVoxelVectorBuffer Result;
	Result.X = FVoxelFloatBuffer::Make(ResultX);
	Result.Y = FVoxelFloatBuffer::Make(ResultY);
	Result.Z = FVoxelFloatBuffer::Make(ResultZ);
	return Result;
}

FVoxelVectorBuffer FVoxelBufferUtilities::ApplyInverseTransform(const FVoxelVectorBuffer& Buffer, const FTransform& Transform)
{
	if (Transform.Equals(FTransform::Identity))
	{
		return Buffer;
	}

	VOXEL_FUNCTION_COUNTER_NUM(Buffer.Num(), 1024);

	FVoxelFloatBufferStorage ResultX; ResultX.Allocate(Buffer.Num());
	FVoxelFloatBufferStorage ResultY; ResultY.Allocate(Buffer.Num());
	FVoxelFloatBufferStorage ResultZ; ResultZ.Allocate(Buffer.Num());

	ForeachVoxelBufferChunk(Buffer.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_ApplyInverseTransform(
			Buffer.X.GetData(Iterator), Buffer.X.IsConstant(),
			Buffer.Y.GetData(Iterator), Buffer.Y.IsConstant(),
			Buffer.Z.GetData(Iterator), Buffer.Z.IsConstant(),
			Iterator.Num(),
			GetISPCValue(FVector3f(Transform.GetTranslation())),
			GetISPCValue(FVector4f(
				Transform.GetRotation().X,
				Transform.GetRotation().Y,
				Transform.GetRotation().Z,
				Transform.GetRotation().W)),
			GetISPCValue(FVector3f(Transform.GetScale3D())),
			ResultX.GetData(Iterator),
			ResultY.GetData(Iterator),
			ResultZ.GetData(Iterator));
	});

	FVoxelVectorBuffer Result;
	Result.X = FVoxelFloatBuffer::Make(ResultX);
	Result.Y = FVoxelFloatBuffer::Make(ResultY);
	Result.Z = FVoxelFloatBuffer::Make(ResultZ);
	return Result;
}

FVoxelVectorBuffer FVoxelBufferUtilities::ApplyTransform(
	const FVoxelVectorBuffer& Buffer,
	const FVoxelVectorBuffer& Translation,
	const FVoxelQuaternionBuffer& Rotation,
	const FVoxelVectorBuffer& Scale)
{
	const FVoxelBufferAccessor BufferAccessor(Buffer, Translation, Rotation, Scale);
	if (!ensure(BufferAccessor.IsValid()))
	{
		return {};
	}

	VOXEL_FUNCTION_COUNTER_NUM(BufferAccessor.Num(), 1024);

	FVoxelFloatBufferStorage ResultX; ResultX.Allocate(BufferAccessor.Num());
	FVoxelFloatBufferStorage ResultY; ResultY.Allocate(BufferAccessor.Num());
	FVoxelFloatBufferStorage ResultZ; ResultZ.Allocate(BufferAccessor.Num());

	ForeachVoxelBufferChunk(BufferAccessor.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_ApplyTransform_Bulk(
			Buffer.X.GetData(Iterator), Buffer.X.IsConstant(),
			Buffer.Y.GetData(Iterator), Buffer.Y.IsConstant(),
			Buffer.Z.GetData(Iterator), Buffer.Z.IsConstant(),
			Translation.X.GetData(Iterator), Translation.X.IsConstant(),
			Translation.Y.GetData(Iterator), Translation.Y.IsConstant(),
			Translation.Z.GetData(Iterator), Translation.Z.IsConstant(),
			Rotation.X.GetData(Iterator), Rotation.X.IsConstant(),
			Rotation.Y.GetData(Iterator), Rotation.Y.IsConstant(),
			Rotation.Z.GetData(Iterator), Rotation.Z.IsConstant(),
			Rotation.W.GetData(Iterator), Rotation.W.IsConstant(),
			Scale.X.GetData(Iterator), Scale.X.IsConstant(),
			Scale.Y.GetData(Iterator), Scale.Y.IsConstant(),
			Scale.Z.GetData(Iterator), Scale.Z.IsConstant(),
			Iterator.Num(),
			ResultX.GetData(Iterator),
			ResultY.GetData(Iterator),
			ResultZ.GetData(Iterator));
	});

	return FVoxelVectorBuffer::Make(ResultX, ResultY, ResultZ);
}

FVoxelVectorBuffer FVoxelBufferUtilities::ApplyTransform(const FVoxelVectorBuffer& Buffer, const FMatrix& Matrix)
{
	const FTransform Transform(Matrix);
	ensure(Transform.ToMatrixWithScale().Equals(Matrix));
	return ApplyTransform(Buffer, Transform);
}

FVoxelFloatBuffer FVoxelBufferUtilities::TransformDistance(const FVoxelFloatBuffer& Distance, const FMatrix& Transform)
{
	const float Scale = Transform.GetScaleVector().GetAbsMax();
	if (Scale == 1.f)
	{
		return Distance;
	}

	VOXEL_FUNCTION_COUNTER_NUM(Distance.Num(), 1024);

	FVoxelFloatBufferStorage Result;
	Result.Allocate(Distance.Num());

	ForeachVoxelBufferChunk(Distance.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_TransformDistance(
			Distance.GetData(Iterator),
			Result.GetData(Iterator),
			Scale,
			Iterator.Num());
	});

	return FVoxelFloatBuffer::Make(Result);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFloatBuffer FVoxelBufferUtilities::Min8(
	const FVoxelFloatBuffer& Buffer0,
	const FVoxelFloatBuffer& Buffer1,
	const FVoxelFloatBuffer& Buffer2,
	const FVoxelFloatBuffer& Buffer3,
	const FVoxelFloatBuffer& Buffer4,
	const FVoxelFloatBuffer& Buffer5,
	const FVoxelFloatBuffer& Buffer6,
	const FVoxelFloatBuffer& Buffer7)
{
	const FVoxelBufferAccessor BufferAccessor(
		Buffer0,
		Buffer1,
		Buffer2,
		Buffer3,
		Buffer4,
		Buffer5,
		Buffer6,
		Buffer7);
	if (!ensure(BufferAccessor.IsValid()))
	{
		return {};
	}

	VOXEL_FUNCTION_COUNTER_NUM(BufferAccessor.Num(), 1024);

	FVoxelFloatBufferStorage Result;
	Result.Allocate(BufferAccessor.Num());

	ForeachVoxelBufferChunk(BufferAccessor.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_Min8(
			Buffer0.GetData(Iterator), Buffer0.IsConstant(),
			Buffer1.GetData(Iterator), Buffer1.IsConstant(),
			Buffer2.GetData(Iterator), Buffer2.IsConstant(),
			Buffer3.GetData(Iterator), Buffer3.IsConstant(),
			Buffer4.GetData(Iterator), Buffer4.IsConstant(),
			Buffer5.GetData(Iterator), Buffer5.IsConstant(),
			Buffer6.GetData(Iterator), Buffer6.IsConstant(),
			Buffer7.GetData(Iterator), Buffer7.IsConstant(),
			Result.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelFloatBuffer::Make(Result);
}

FVoxelFloatBuffer FVoxelBufferUtilities::Max8(
	const FVoxelFloatBuffer& Buffer0,
	const FVoxelFloatBuffer& Buffer1,
	const FVoxelFloatBuffer& Buffer2,
	const FVoxelFloatBuffer& Buffer3,
	const FVoxelFloatBuffer& Buffer4,
	const FVoxelFloatBuffer& Buffer5,
	const FVoxelFloatBuffer& Buffer6,
	const FVoxelFloatBuffer& Buffer7)
{
	const FVoxelBufferAccessor BufferAccessor(
		Buffer0,
		Buffer1,
		Buffer2,
		Buffer3,
		Buffer4,
		Buffer5,
		Buffer6,
		Buffer7);
	if (!ensure(BufferAccessor.IsValid()))
	{
		return {};
	}

	VOXEL_FUNCTION_COUNTER_NUM(BufferAccessor.Num(), 1024);

	FVoxelFloatBufferStorage Result;
	Result.Allocate(BufferAccessor.Num());

	ForeachVoxelBufferChunk(BufferAccessor.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_Max8(
			Buffer0.GetData(Iterator), Buffer0.IsConstant(),
			Buffer1.GetData(Iterator), Buffer1.IsConstant(),
			Buffer2.GetData(Iterator), Buffer2.IsConstant(),
			Buffer3.GetData(Iterator), Buffer3.IsConstant(),
			Buffer4.GetData(Iterator), Buffer4.IsConstant(),
			Buffer5.GetData(Iterator), Buffer5.IsConstant(),
			Buffer6.GetData(Iterator), Buffer6.IsConstant(),
			Buffer7.GetData(Iterator), Buffer7.IsConstant(),
			Result.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelFloatBuffer::Make(Result);
}

FVoxelVectorBuffer FVoxelBufferUtilities::Min8(
	const FVoxelVectorBuffer& Buffer0,
	const FVoxelVectorBuffer& Buffer1,
	const FVoxelVectorBuffer& Buffer2,
	const FVoxelVectorBuffer& Buffer3,
	const FVoxelVectorBuffer& Buffer4,
	const FVoxelVectorBuffer& Buffer5,
	const FVoxelVectorBuffer& Buffer6,
	const FVoxelVectorBuffer& Buffer7)
{
	FVoxelVectorBuffer Result;
	Result.X = Min8(Buffer0.X, Buffer1.X, Buffer2.X, Buffer3.X, Buffer4.X, Buffer5.X, Buffer6.X, Buffer7.X);
	Result.Y = Min8(Buffer0.Y, Buffer1.Y, Buffer2.Y, Buffer3.Y, Buffer4.Y, Buffer5.Y, Buffer6.Y, Buffer7.Y);
	Result.Z = Min8(Buffer0.Z, Buffer1.Z, Buffer2.Z, Buffer3.Z, Buffer4.Z, Buffer5.Z, Buffer6.Z, Buffer7.Z);
	return Result;
}

FVoxelVectorBuffer FVoxelBufferUtilities::Max8(
	const FVoxelVectorBuffer& Buffer0,
	const FVoxelVectorBuffer& Buffer1,
	const FVoxelVectorBuffer& Buffer2,
	const FVoxelVectorBuffer& Buffer3,
	const FVoxelVectorBuffer& Buffer4,
	const FVoxelVectorBuffer& Buffer5,
	const FVoxelVectorBuffer& Buffer6,
	const FVoxelVectorBuffer& Buffer7)
{
	FVoxelVectorBuffer Result;
	Result.X = Max8(Buffer0.X, Buffer1.X, Buffer2.X, Buffer3.X, Buffer4.X, Buffer5.X, Buffer6.X, Buffer7.X);
	Result.Y = Max8(Buffer0.Y, Buffer1.Y, Buffer2.Y, Buffer3.Y, Buffer4.Y, Buffer5.Y, Buffer6.Y, Buffer7.Y);
	Result.Z = Max8(Buffer0.Z, Buffer1.Z, Buffer2.Z, Buffer3.Z, Buffer4.Z, Buffer5.Z, Buffer6.Z, Buffer7.Z);
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFloatBuffer FVoxelBufferUtilities::IntToFloat(const FVoxelInt32Buffer& Buffer)
{
	VOXEL_FUNCTION_COUNTER_NUM(Buffer.Num(), 1024);

	FVoxelFloatBufferStorage Result;
	Result.Allocate(Buffer.Num());

	ForeachVoxelBufferChunk(Buffer.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_IntToFloat(
			Buffer.GetData(Iterator),
			Result.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelFloatBuffer::Make(Result);
}

FVoxelDoubleBuffer FVoxelBufferUtilities::IntToDouble(const FVoxelInt32Buffer& Buffer)
{
	VOXEL_FUNCTION_COUNTER_NUM(Buffer.Num(), 1024);

	FVoxelDoubleBufferStorage Result;
	Result.Allocate(Buffer.Num());

	ForeachVoxelBufferChunk(Buffer.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_IntToDouble(
			Buffer.GetData(Iterator),
			Result.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelDoubleBuffer::Make(Result);
}

FVoxelDoubleBuffer FVoxelBufferUtilities::FloatToDouble(const FVoxelFloatBuffer& Float)
{
	VOXEL_FUNCTION_COUNTER_NUM(Float.Num(), 1024);

	FVoxelDoubleBufferStorage Result;
	Result.Allocate(Float.Num());

	ForeachVoxelBufferChunk(Float.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_FloatToDouble(
			Float.GetData(Iterator),
			Result.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelDoubleBuffer::Make(Result);
}

FVoxelFloatBuffer FVoxelBufferUtilities::DoubleToFloat(const FVoxelDoubleBuffer& Double)
{
	VOXEL_FUNCTION_COUNTER_NUM(Double.Num(), 1024);

	FVoxelFloatBufferStorage Result;
	Result.Allocate(Double.Num());

	ForeachVoxelBufferChunk(Double.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_DoubleToFloat(
			Double.GetData(Iterator),
			Result.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelFloatBuffer::Make(Result);
}

FVoxelSeedBuffer FVoxelBufferUtilities::PointIdToSeed(const FVoxelPointIdBuffer& Buffer)
{
	VOXEL_FUNCTION_COUNTER_NUM(Buffer.Num(), 1024);

	FVoxelSeedBufferStorage Result;
	Result.Allocate(Buffer.Num());

	ForeachVoxelBufferChunk(Buffer.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_PointIdToSeed(
			ReinterpretCastPtr<uint64>(Buffer.GetData(Iterator)),
			ReinterpretCastPtr<uint32>(Result.GetData(Iterator)),
			Iterator.Num());
	});

	return FVoxelSeedBuffer::Make(Result);
}

FVoxelInt32Buffer FVoxelBufferUtilities::BoolToInt32(const FVoxelBoolBuffer& Buffer)
{
	VOXEL_FUNCTION_COUNTER_NUM(Buffer.Num(), 1024);

	FVoxelInt32BufferStorage Result;
	Result.Allocate(Buffer.Num());

	ForeachVoxelBufferChunk(Buffer.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_BoolToInt32(
			Buffer.GetData(Iterator),
			Result.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelInt32Buffer::Make(Result);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFloatBuffer FVoxelBufferUtilities::Add(const FVoxelFloatBuffer& A, const FVoxelFloatBuffer& B)
{
	const FVoxelBufferAccessor BufferAccessor(A, B);
	if (!ensure(BufferAccessor.IsValid()))
	{
		return {};
	}

	VOXEL_FUNCTION_COUNTER_NUM(BufferAccessor.Num(), 1024);

	FVoxelFloatBufferStorage Result;
	Result.Allocate(BufferAccessor.Num());

	ForeachVoxelBufferChunk(BufferAccessor.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_Add(
			A.GetData(Iterator),
			A.IsConstant(),
			B.GetData(Iterator),
			B.IsConstant(),
			Result.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelFloatBuffer::Make(Result);
}

FVoxelFloatBuffer FVoxelBufferUtilities::Multiply(const FVoxelFloatBuffer& A, const FVoxelFloatBuffer& B)
{
	const FVoxelBufferAccessor BufferAccessor(A, B);
	if (!ensure(BufferAccessor.IsValid()))
	{
		return {};
	}

	VOXEL_FUNCTION_COUNTER_NUM(BufferAccessor.Num(), 1024);

	FVoxelFloatBufferStorage Result;
	Result.Allocate(BufferAccessor.Num());

	ForeachVoxelBufferChunk(BufferAccessor.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_Multiply(
			A.GetData(Iterator),
			A.IsConstant(),
			B.GetData(Iterator),
			B.IsConstant(),
			Result.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelFloatBuffer::Make(Result);
}

FVoxelVectorBuffer FVoxelBufferUtilities::Add(const FVoxelVectorBuffer& A, const FVoxelVectorBuffer& B)
{
	FVoxelVectorBuffer Result;
	Result.X = Add(A.X, B.X);
	Result.Y = Add(A.Y, B.Y);
	Result.Z = Add(A.Z, B.Z);
	return Result;
}

FVoxelVectorBuffer FVoxelBufferUtilities::Multiply(const FVoxelVectorBuffer& A, const FVoxelVectorBuffer& B)
{
	FVoxelVectorBuffer Result;
	Result.X = Multiply(A.X, B.X);
	Result.Y = Multiply(A.Y, B.Y);
	Result.Z = Multiply(A.Z, B.Z);
	return Result;
}

FVoxelQuaternionBuffer FVoxelBufferUtilities::Combine(const FVoxelQuaternionBuffer& A, const FVoxelQuaternionBuffer& B)
{
	const FVoxelBufferAccessor BufferAccessor(A, B);
	if (!ensure(BufferAccessor.IsValid()))
	{
		return {};
	}

	VOXEL_FUNCTION_COUNTER_NUM(BufferAccessor.Num(), 1024);

	FVoxelFloatBufferStorage X;
	FVoxelFloatBufferStorage Y;
	FVoxelFloatBufferStorage Z;
	FVoxelFloatBufferStorage W;
	X.Allocate(BufferAccessor.Num());
	Y.Allocate(BufferAccessor.Num());
	Z.Allocate(BufferAccessor.Num());
	W.Allocate(BufferAccessor.Num());

	ForeachVoxelBufferChunk(BufferAccessor.Num(), [&](const FVoxelBufferIterator& Iterator)
	{
		ispc::VoxelBufferUtilities_Combine(
			A.X.GetData(Iterator),
			A.X.IsConstant(),
			A.Y.GetData(Iterator),
			A.Y.IsConstant(),
			A.Z.GetData(Iterator),
			A.Z.IsConstant(),
			A.W.GetData(Iterator),
			A.W.IsConstant(),
			B.X.GetData(Iterator),
			B.X.IsConstant(),
			B.Y.GetData(Iterator),
			B.Y.IsConstant(),
			B.Z.GetData(Iterator),
			B.Z.IsConstant(),
			B.W.GetData(Iterator),
			B.W.IsConstant(),
			X.GetData(Iterator),
			Y.GetData(Iterator),
			Z.GetData(Iterator),
			W.GetData(Iterator),
			Iterator.Num());
	});

	return FVoxelQuaternionBuffer::Make(X, Y, Z, W);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBufferUtilities::MakePalette(
	const FVoxelTerminalBuffer& Buffer,
	FVoxelInt32Buffer& OutIndices,
	FVoxelTerminalBuffer& OutPalette)
{
	VOXEL_FUNCTION_COUNTER_NUM(Buffer.Num(), 1024);
	check(Buffer.GetStruct() == OutPalette.GetStruct());

	if (Buffer.Num() == 0)
	{
		OutIndices = FVoxelInt32Buffer::MakeEmpty();
		OutPalette.SetAsEmpty();
		return;
	}
	if (Buffer.IsConstant())
	{
		OutIndices = FVoxelInt32Buffer::Make(0);
		Buffer.CopyTo(OutPalette);
		return;
	}

	if (Buffer.IsA<FVoxelSimpleTerminalBuffer>())
	{
		const FVoxelSimpleTerminalBuffer& SimpleBuffer = CastChecked<FVoxelSimpleTerminalBuffer>(Buffer);
		FVoxelSimpleTerminalBuffer& OutSimplePalette = CastChecked<FVoxelSimpleTerminalBuffer>(OutPalette);

		FVoxelInt32BufferStorage Indices;
		Indices.Allocate(Buffer.Num());

		const TSharedRef<FVoxelBufferStorage> Palette = OutSimplePalette.MakeNewStorage();

		VOXEL_SWITCH_TERMINAL_TYPE_SIZE(SimpleBuffer.GetTypeSize())
		{
			using Type = VOXEL_GET_TYPE(TypeInstance);

			TVoxelAddOnlyChunkedMap<Type, int32> ValueToIndex;
			ValueToIndex.Reserve(Buffer.Num());

			for (int32 Index = 0; Index < Buffer.Num(); Index++)
			{
				const Type Value = SimpleBuffer.GetStorage<Type>()[Index];

				if (const int32* ExistingIndex = ValueToIndex.Find(Value))
				{
					Indices[Index] = *ExistingIndex;
				}
				else
				{
					const int32 NewIndex = Palette->As<Type>().Add(Value);
					ValueToIndex.Add_CheckNew_NoRehash(Value, NewIndex);
					Indices[Index] = NewIndex;
				}
			}

			OutIndices.SetStorage(Indices);
			OutSimplePalette.SetStorage(Palette);
		};
	}
	else
	{
		// Not supported
		ensure(false);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBufferUtilities::Gather(
	FVoxelTerminalBuffer& OutBuffer,
	const FVoxelTerminalBuffer& Buffer,
	const FVoxelInt32Buffer& Indices)
{
	VOXEL_SCOPE_COUNTER_FORMAT_COND(Indices.Num() > 1024, "FVoxelBufferUtilities::Gather Num=%d", Indices.Num());
	check(OutBuffer.GetStruct() == Buffer.GetStruct());

	if (Buffer.IsConstant())
	{
		Buffer.CopyTo(OutBuffer);
		return;
	}
	if (Indices.Num() == 0)
	{
		OutBuffer.SetAsEmpty();
		return;
	}

	if (Indices.Num() == 1)
	{
		const int32 Index = Indices[0];
		if (Index == -1)
		{
			OutBuffer.InitializeFromConstant(FVoxelRuntimePinValue(OutBuffer.GetInnerType()));
		}
		else
		{
			OutBuffer.InitializeFromConstant(Buffer.GetGeneric(Index));
		}
		return;
	}

	if (Buffer.IsA<FVoxelSimpleTerminalBuffer>())
	{
		FVoxelSimpleTerminalBuffer& OutSimpleBuffer = CastChecked<FVoxelSimpleTerminalBuffer>(OutBuffer);
		const FVoxelSimpleTerminalBuffer& SimpleBuffer = CastChecked<FVoxelSimpleTerminalBuffer>(Buffer);
		const int32 TypeSize = SimpleBuffer.GetTypeSize();

		const TSharedRef<FVoxelBufferStorage> Storage = OutSimpleBuffer.MakeNewStorage();
		Storage->Allocate(Indices.Num());

		ForeachVoxelBufferChunk(Indices.Num(), [&](const FVoxelBufferIterator& Iterator)
		{
			const TConstVoxelArrayView<int32> IndicesView = Indices.GetRawView_NotConstant(Iterator);

			VOXEL_SWITCH_TERMINAL_TYPE_SIZE(TypeSize)
			{
				using Type = VOXEL_GET_TYPE(TypeInstance);

				const TVoxelArrayView<Type> WriteView = Storage->As<Type>().GetRawView_NotConstant(Iterator);
				const TVoxelBufferStorage<Type>& ReadView = SimpleBuffer.GetStorage<Type>();

				for (int32 WriteIndex = 0; WriteIndex < IndicesView.Num(); WriteIndex++)
				{
					const int32 ReadIndex = IndicesView[WriteIndex];
					WriteView[WriteIndex] = ReadIndex == -1 ? 0 : ReadView[ReadIndex];
				}
			};
		});

		OutSimpleBuffer.SetStorage(Storage);
	}
	else
	{
		FVoxelComplexTerminalBuffer& OutComplexBuffer = CastChecked<FVoxelComplexTerminalBuffer>(OutBuffer);
		const FVoxelComplexTerminalBuffer& ComplexBuffer = CastChecked<FVoxelComplexTerminalBuffer>(Buffer);

		const TSharedRef<FVoxelComplexBufferStorage> Storage = OutComplexBuffer.MakeNewStorage();
		Storage->Allocate(Indices.Num());

		for (int32 WriteIndex = 0; WriteIndex < Indices.Num(); WriteIndex++)
		{
			const int32 ReadIndex = Indices[WriteIndex];
			if (ReadIndex == -1)
			{
				continue;
			}

			ComplexBuffer.GetStorage()[ReadIndex].CopyTo((*Storage)[WriteIndex]);
		}

		OutComplexBuffer.SetStorage(Storage);
	}
}

TSharedRef<const FVoxelBuffer> FVoxelBufferUtilities::Gather(
	const FVoxelBuffer& Buffer,
	const FVoxelInt32Buffer& Indices)
{
	VOXEL_FUNCTION_COUNTER_NUM(Indices.Num(), 1024);

	const TSharedRef<FVoxelBuffer> NewBuffer = MakeSharedStruct<FVoxelBuffer>(Buffer.GetStruct());
	check(Buffer.NumTerminalBuffers() == NewBuffer->NumTerminalBuffers());
	for (int32 Index = 0; Index < Buffer.NumTerminalBuffers(); Index++)
	{
		Gather(
			NewBuffer->GetTerminalBuffer(Index),
			Buffer.GetTerminalBuffer(Index),
			Indices);
	}
	return NewBuffer;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBufferUtilities::Replicate(
	FVoxelTerminalBuffer& OutBuffer,
	const FVoxelTerminalBuffer& Buffer,
	const TConstVoxelArrayView<int32> Counts,
	const int32 NewNum)
{
	VOXEL_SCOPE_COUNTER_FORMAT_COND(NewNum > 1024, "FVoxelBufferUtilities::Replicate Num=%d", NewNum);

	if (NewNum == 0)
	{
		return;
	}
	if (Buffer.IsConstant())
	{
		Buffer.CopyTo(OutBuffer);
		return;
	}

	if (VOXEL_DEBUG)
	{
		int32 ActualNum = 0;
		for (const int32 Count : Counts)
		{
			ActualNum += Count;
		}
		check(NewNum == ActualNum);
	}

	if (Buffer.IsA<FVoxelSimpleTerminalBuffer>())
	{
		FVoxelSimpleTerminalBuffer& OutSimpleBuffer = CastChecked<FVoxelSimpleTerminalBuffer>(OutBuffer);
		const FVoxelSimpleTerminalBuffer& SimpleBuffer = CastChecked<FVoxelSimpleTerminalBuffer>(Buffer);
		const int32 TypeSize = SimpleBuffer.GetTypeSize();

		const TSharedRef<FVoxelBufferStorage> Storage = OutSimpleBuffer.MakeNewStorage();
		Storage->Allocate(NewNum);

		VOXEL_SWITCH_TERMINAL_TYPE_SIZE(TypeSize)
		{
			using Type = VOXEL_GET_TYPE(TypeInstance);

			int32 WriteIndex = 0;
			for (int32 ReadIndex = 0; ReadIndex < Counts.Num(); ReadIndex++)
			{
				const int32 Count = Counts[ReadIndex];
				const Type Value = SimpleBuffer.GetStorage<Type>()[ReadIndex];

				FVoxelBufferIterator Iterator;
				Iterator.Initialize(WriteIndex + Count, WriteIndex);
				for (; Iterator; ++Iterator)
				{
					FVoxelUtilities::SetAll(
						Storage->As<Type>().GetRawView_NotConstant(Iterator),
						Value);
				}

				WriteIndex += Count;
			}
			ensure(WriteIndex == NewNum);
		};

		OutSimpleBuffer.SetStorage(Storage);
	}
	else
	{
		FVoxelComplexTerminalBuffer& OutComplexBuffer = CastChecked<FVoxelComplexTerminalBuffer>(OutBuffer);
		const FVoxelComplexTerminalBuffer& ComplexBuffer = CastChecked<FVoxelComplexTerminalBuffer>(Buffer);

		const TSharedRef<FVoxelComplexBufferStorage> Storage = OutComplexBuffer.MakeNewStorage();
		Storage->Allocate(NewNum);

		int32 WriteIndex = 0;
		for (int32 ReadIndex = 0; ReadIndex < Counts.Num(); ReadIndex++)
		{
			const int32 Count = Counts[ReadIndex];
			const FConstVoxelStructView Value = ComplexBuffer.GetStorage()[ReadIndex];

			for (int32 Index = 0; Index < Count; Index++)
			{
				Value.CopyTo((*Storage)[WriteIndex++]);
			}
		}
		ensure(WriteIndex == NewNum);

		OutComplexBuffer.SetStorage(Storage);
	}
}

TSharedRef<const FVoxelBuffer> FVoxelBufferUtilities::Replicate(
	const FVoxelBuffer& Buffer,
	const TConstVoxelArrayView<int32> Counts,
	const int32 NewNum)
{
	VOXEL_FUNCTION_COUNTER_NUM(Buffer.Num(), 1024);

	if (VOXEL_DEBUG)
	{
		int32 NewNumCheck = 0;
		for (const int32 Count : Counts)
		{
			NewNumCheck += Count;
		}
		check(NewNum == NewNumCheck);
	}

	const TSharedRef<FVoxelBuffer> NewBuffer = MakeSharedStruct<FVoxelBuffer>(Buffer.GetStruct());

	for (int32 Index = 0; Index < NewBuffer->NumTerminalBuffers(); Index++)
	{
		Replicate(
			NewBuffer->GetTerminalBuffer(Index),
			Buffer.GetTerminalBuffer(Index),
			Counts,
			NewNum);
	}

	return NewBuffer;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFutureValue FVoxelBufferUtilities::Query2D(
	const FVoxelComputeValue& ComputeValue,
	const FVoxelQuery& Query,
	const FVoxelPinType& BufferType)
{
	const FVoxelPositionQueryParameter* PositionQueryParameter = Query.GetParameters().Find<FVoxelPositionQueryParameter>();
	if (!PositionQueryParameter ||
		!PositionQueryParameter->IsGrid3D())
	{
		return ComputeValue(Query);
	}

	const FVoxelPositionQueryParameter::FGrid3D Grid3D = PositionQueryParameter->GetGrid3D();
	if (Grid3D.Size.Z <= 1)
	{
		return ComputeValue(Query);
	}

	const TSharedRef<FVoxelQueryParameters> Parameters = Query.CloneParameters();
	Parameters->Add<FVoxelPositionQueryParameter>().InitializeGrid2D(
		FVector2f(Grid3D.Start),
		Grid3D.Step,
		FIntPoint(Grid3D.Size.X, Grid3D.Size.Y));

	const FVoxelFutureValue FutureDataBuffer = ComputeValue(Query.MakeNewQuery(Parameters));
	if (!ensure(FutureDataBuffer.GetParentType().IsBuffer()))
	{
		return ComputeValue(Query);
	}

	return
		MakeVoxelTask(STATIC_FNAME("Query2D"))
		.Dependency(FutureDataBuffer)
		.Execute(FutureDataBuffer.GetParentType(), [=]
		{
			const FVoxelBuffer& DataBuffer = FutureDataBuffer.Get_CheckCompleted<FVoxelBuffer>();
			if (DataBuffer.IsConstant())
			{
				return FVoxelRuntimePinValue::Make(FutureDataBuffer.GetShared_CheckCompleted<FVoxelBuffer>(), BufferType);
			}

			const TSharedRef<FVoxelBuffer> Result = MakeSharedStruct<FVoxelBuffer>(DataBuffer.GetStruct());
			check(DataBuffer.NumTerminalBuffers() == Result->NumTerminalBuffers());
			for (int32 Index = 0; Index < DataBuffer.NumTerminalBuffers(); Index++)
			{
				ExpandQuery2D(
					Result->GetTerminalBuffer(Index),
					DataBuffer.GetTerminalBuffer(Index),
					Grid3D.Size.Z);
			}
			return FVoxelRuntimePinValue::Make(Result, BufferType);
		});
}

void FVoxelBufferUtilities::ExpandQuery2D(
	FVoxelTerminalBuffer& OutBuffer,
	const FVoxelTerminalBuffer& Buffer,
	const int32 Count)
{
	VOXEL_FUNCTION_COUNTER_NUM(Buffer.Num() * Count, 1024);
	check(OutBuffer.GetStruct() == Buffer.GetStruct());

	if (Buffer.IsConstant())
	{
		Buffer.CopyTo(OutBuffer);
		return;
	}
	ensure(Count >= 2);
	ensure(Count % 2 == 0);

	if (Buffer.IsA<FVoxelSimpleTerminalBuffer>())
	{
		FVoxelSimpleTerminalBuffer& OutSimpleBuffer = CastChecked<FVoxelSimpleTerminalBuffer>(OutBuffer);
		const FVoxelSimpleTerminalBuffer& SimpleBuffer = CastChecked<FVoxelSimpleTerminalBuffer>(Buffer);
		const int32 TypeSize = SimpleBuffer.GetTypeSize();

		const int32 Num = Buffer.Num();
		ensure(Num % 4 == 0);

		const TSharedRef<FVoxelBufferStorage> Storage = OutSimpleBuffer.MakeNewStorage();
		Storage->Allocate(Num * Count);

		if (TypeSize == sizeof(float))
		{
			ForeachVoxelBufferChunk(Num * 2, [&](const FVoxelBufferIterator& Iterator)
			{
				ensure(Iterator.Num() % 8 == 0);

				ispc::VoxelBufferUtilities_ReplicatePacked(
					SimpleBuffer.GetStorage<float>().GetData(Iterator.GetHalfIterator()),
					Storage->As<float>().GetData(Iterator),
					Iterator.Num() / 2);
			});
		}
		else
		{
			ForeachVoxelBufferChunk(Num * 2, [&](const FVoxelBufferIterator& Iterator)
			{
				VOXEL_SWITCH_TERMINAL_TYPE_SIZE(TypeSize)
				{
					using Type = VOXEL_GET_TYPE(TypeInstance);

					const TVoxelArrayView<Type> WriteView = Storage->As<Type>().GetRawView_NotConstant(Iterator);
					const TConstVoxelArrayView<Type> ReadView = SimpleBuffer.GetStorage<Type>().GetRawView_NotConstant(Iterator.GetHalfIterator());

					ensure(Iterator.Num() % 8 == 0);
					for (int32 Index = 0; Index < Iterator.Num() / 8; Index++)
					{
						WriteView[8 * Index + 0] = ReadView[4 * Index + 0];
						WriteView[8 * Index + 1] = ReadView[4 * Index + 1];
						WriteView[8 * Index + 2] = ReadView[4 * Index + 2];
						WriteView[8 * Index + 3] = ReadView[4 * Index + 3];

						WriteView[8 * Index + 4] = ReadView[4 * Index + 0];
						WriteView[8 * Index + 5] = ReadView[4 * Index + 1];
						WriteView[8 * Index + 6] = ReadView[4 * Index + 2];
						WriteView[8 * Index + 7] = ReadView[4 * Index + 3];
					}
				};
			});
		}

		for (int32 Index = 2; Index < Count; Index += 2)
		{
			VOXEL_SCOPE_COUNTER_COND(Num > 4096, "Memcpy");

			TVoxelBufferMultiIterator<2> Iterator((Index + 2) * Num, 2 * Num);
			Iterator.SetIndex<0>(Index * Num);

			for (; Iterator; ++Iterator)
			{
				FVoxelUtilities::Memcpy(
					Storage->GetByteRawView_NotConstant(Iterator.Get<0>()),
					Storage->GetByteRawView_NotConstant(Iterator.Get<1>()));
			}
		}

		OutSimpleBuffer.SetStorage(Storage);
	}
	else
	{
		FVoxelComplexTerminalBuffer& OutComplexBuffer = CastChecked<FVoxelComplexTerminalBuffer>(OutBuffer);
		const FVoxelComplexTerminalBuffer& ComplexBuffer = CastChecked<FVoxelComplexTerminalBuffer>(Buffer);

		const int32 Num = Buffer.Num();
		ensure(Num % 4 == 0);

		const TSharedRef<FVoxelComplexBufferStorage> Storage = OutComplexBuffer.MakeNewStorage();
		Storage->Allocate(Num * Count);

		for (int32 Index = 0; Index < Num / 4; Index++)
		{
			const FConstVoxelStructView Value0 = ComplexBuffer.GetStorage()[4 * Index + 0];
			const FConstVoxelStructView Value1 = ComplexBuffer.GetStorage()[4 * Index + 1];
			const FConstVoxelStructView Value2 = ComplexBuffer.GetStorage()[4 * Index + 2];
			const FConstVoxelStructView Value3 = ComplexBuffer.GetStorage()[4 * Index + 3];

			for (int32 Layer = 0; Layer < Count; Layer += 2)
			{
				Value0.CopyTo((*Storage)[Layer * Num + 8 * Index + 0]);
				Value1.CopyTo((*Storage)[Layer * Num + 8 * Index + 1]);
				Value2.CopyTo((*Storage)[Layer * Num + 8 * Index + 2]);
				Value3.CopyTo((*Storage)[Layer * Num + 8 * Index + 3]);

				Value0.CopyTo((*Storage)[Layer * Num + 8 * Index + 4]);
				Value1.CopyTo((*Storage)[Layer * Num + 8 * Index + 5]);
				Value2.CopyTo((*Storage)[Layer * Num + 8 * Index + 6]);
				Value3.CopyTo((*Storage)[Layer * Num + 8 * Index + 7]);
			}
		}

		OutComplexBuffer.SetStorage(Storage);
	}
}