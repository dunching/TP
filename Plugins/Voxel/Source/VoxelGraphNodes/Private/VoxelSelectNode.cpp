// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelSelectNode.h"
#include "VoxelBufferUtilities.h"

FVoxelNode_Select::FVoxelNode_Select()
{
	GetPin(IndexPin).SetType(FVoxelPinType::Make<FVoxelBoolBuffer>());
	FixupValuePins();
}

DEFINE_VOXEL_NODE_COMPUTE(FVoxelNode_Select, Result)
{
	const FVoxelPinType IndexType = GetNodeRuntime().GetPinData(IndexPin).Type;

	if (IndexType.Is<bool>() ||
		IndexType.Is<int32>())
	{
		const FVoxelFutureValue IndexValue = Get(IndexPin, Query);

		return VOXEL_ON_COMPLETE(IndexType, IndexValue)
		{
			int32 Index;
			if (IndexType.Is<bool>())
			{
				Index = IndexValue.Get<bool>() ? 1 : 0;
			}
			else
			{
				Index = IndexValue.Get<int32>();
			}

			if (!ValuePins.IsValidIndex(Index))
			{
				return {};
			}

			return Get(ValuePins[Index], Query);
		};
	}
	else if (
		IndexType.Is<FVoxelBoolBuffer>() ||
		IndexType.Is<FVoxelInt32Buffer>())
	{
		const TValue<FVoxelBuffer> IndexValue = Get<FVoxelBuffer>(IndexPin, Query);

		return VOXEL_ON_COMPLETE(IndexType, IndexValue)
		{
			FVoxelInt32Buffer Indices;
			if (IndexType.Is<FVoxelBoolBuffer>())
			{
				Indices = FVoxelBufferUtilities::BoolToInt32(CastChecked<FVoxelBoolBuffer>(*IndexValue));
			}
			else
			{
				Indices = CastChecked<FVoxelInt32Buffer>(*IndexValue);
			}

			if (Indices.Num() == 0)
			{
				return {};
			}

			if (Indices.IsConstant())
			{
				const int32 Index = Indices.GetConstant();

				if (!ValuePins.IsValidIndex(Index))
				{
					return {};
				}

				return Get(ValuePins[Index], Query);
			}

			TVoxelArray<TValue<FVoxelBuffer>> Buffers;
			Buffers.Reserve(ValuePins.Num());
			for (const FVoxelPinRef& Pin : ValuePins)
			{
				Buffers.Add(Get<FVoxelBuffer>(Pin, Query));
			}

			return VOXEL_ON_COMPLETE(Buffers, Indices)
			{
				const FVoxelPinType BufferType = GetNodeRuntime().GetPinData(ResultPin).Type;

				const TSharedRef<FVoxelBuffer> Result = FVoxelBuffer::Make(BufferType.GetInnerType());
				for (const TSharedRef<const FVoxelBuffer>& Buffer : Buffers)
				{
					CheckVoxelBuffersNum(Indices, *Buffer);
					check(Result->NumTerminalBuffers() == Buffer->NumTerminalBuffers());
				}

				for (int32 Index = 0; Index < Result->NumTerminalBuffers(); Index++)
				{
					TArray<const FVoxelTerminalBuffer*> TerminalBuffers;
					for (const TSharedRef<const FVoxelBuffer>& Buffer : Buffers)
					{
						TerminalBuffers.Add(&Buffer->GetTerminalBuffer(Index));
					}
					Select(Result->GetTerminalBuffer(Index), Indices, TerminalBuffers);
				}

				return FVoxelRuntimePinValue::Make(Result, BufferType);
			};
		};
	}
	else
	{
		ensure(false);
		return {};
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelNode_Select::Select(
	FVoxelTerminalBuffer& OutBuffer,
	const FVoxelInt32Buffer& Indices,
	const TConstVoxelArrayView<const FVoxelTerminalBuffer*> Buffers)
{
	VOXEL_SCOPE_COUNTER_FORMAT_COND(Indices.Num() > 1024, "Select Num=%d", Indices.Num());

	if (!ensure(Indices.Num() >= 2) ||
		!ensure(Buffers.Num() > 0))
	{
		return;
	}

	const FVoxelPinType InnerType = OutBuffer.GetInnerType();
	for (const FVoxelTerminalBuffer* Buffer : Buffers)
	{
		if (!ensure(Buffer->GetInnerType() == InnerType))
		{
			return;
		}
	}

	if (OutBuffer.IsA<FVoxelSimpleTerminalBuffer>())
	{
		FVoxelSimpleTerminalBuffer& OutSimpleBuffer = CastChecked<FVoxelSimpleTerminalBuffer>(OutBuffer);
		const TConstVoxelArrayView<const FVoxelSimpleTerminalBuffer*> SimpleBuffers = ReinterpretCastVoxelArrayView<const FVoxelSimpleTerminalBuffer*>(Buffers);

		const TSharedRef<FVoxelBufferStorage> Storage = OutSimpleBuffer.MakeNewStorage();
		Storage->Allocate(Indices.Num());

		ForeachVoxelBufferChunk(Indices.Num(), [&](const FVoxelBufferIterator& Iterator)
		{
			const TConstVoxelArrayView<int32> IndicesView = Indices.GetRawView_NotConstant(Iterator);

			VOXEL_SWITCH_TERMINAL_TYPE_SIZE(Storage->GetTypeSize())
			{
				using Type = VOXEL_GET_TYPE(TypeInstance);
				const TVoxelArrayView<Type> WriteView = Storage->As<Type>().GetRawView_NotConstant(Iterator);

				for (int32 Index = 0; Index < Iterator.Num(); Index++)
				{
					const int32 BufferIndex = IndicesView[Index];

					if (!SimpleBuffers.IsValidIndex(BufferIndex))
					{
						WriteView[Index] = 0;
						continue;
					}

					const FVoxelSimpleTerminalBuffer* SimpleBuffer = SimpleBuffers[BufferIndex];
					WriteView[Index] = SimpleBuffer->GetStorage<Type>()[Index];
				}
			};
		});

		OutSimpleBuffer.SetStorage(Storage);
	}
	else
	{
		FVoxelComplexTerminalBuffer& OutComplexBuffer = CastChecked<FVoxelComplexTerminalBuffer>(OutBuffer);
		const TConstVoxelArrayView<const FVoxelComplexTerminalBuffer*> ComplexBuffers = ReinterpretCastVoxelArrayView<const FVoxelComplexTerminalBuffer*>(Buffers);

		const TSharedRef<FVoxelComplexBufferStorage> Storage = OutComplexBuffer.MakeNewStorage();
		Storage->Allocate(Indices.Num());

		for (int32 Index = 0; Index < Indices.Num(); Index++)
		{
			const int32 BufferIndex = Indices[Index];

			if (!ComplexBuffers.IsValidIndex(BufferIndex))
			{
				continue;
			}

			const FVoxelComplexTerminalBuffer* ComplexBuffer = ComplexBuffers[BufferIndex];
			ComplexBuffer->GetStorage()[Index].CopyTo((*Storage)[Index]);
		}

		OutComplexBuffer.SetStorage(Storage);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
FVoxelPinTypeSet FVoxelNode_Select::GetPromotionTypes(const FVoxelPin& Pin) const
{
	if (Pin.Name == IndexPin)
	{
		FVoxelPinTypeSet OutTypes;

		OutTypes.Add<bool>();
		OutTypes.Add<FVoxelBoolBuffer>();

		OutTypes.Add<int32>();
		OutTypes.Add<FVoxelInt32Buffer>();

		return OutTypes;
	}
	else
	{
		return FVoxelPinTypeSet::All();
	}
}

void FVoxelNode_Select::PromotePin(FVoxelPin& Pin, const FVoxelPinType& NewType)
{
	if (Pin.Name == IndexPin)
	{
		GetPin(IndexPin).SetType(NewType);

		const bool bIndexIsBuffer = GetPin(IndexPin).GetType().IsBuffer();
		const bool bResultIsBuffer = GetPin(ResultPin).GetType().IsBuffer();

		if (bIndexIsBuffer != bResultIsBuffer)
		{
			const FVoxelPinType ResultType = GetPin(ResultPin).GetType();
			if (bIndexIsBuffer)
			{
				GetPin(ResultPin).SetType(ResultType.GetBufferType());
			}
			else
			{
				GetPin(ResultPin).SetType(ResultType.GetInnerType());
			}
		}
	}
	else
	{
		GetPin(ResultPin).SetType(NewType);

		for (FVoxelPinRef& ValuePin : ValuePins)
		{
			GetPin(ValuePin).SetType(NewType);
		}

		const bool bIndexIsBuffer = GetPin(IndexPin).GetType().IsBuffer();
		const bool bResultIsBuffer = GetPin(ResultPin).GetType().IsBuffer();

		if (bIndexIsBuffer != bResultIsBuffer)
		{
			const FVoxelPinType IndexType = GetPin(IndexPin).GetType();
			GetPin(IndexPin).SetType(bResultIsBuffer ? IndexType.GetBufferType() : IndexType.GetInnerType());
		}
	}

	FixupValuePins();
}
#endif

void FVoxelNode_Select::PreSerialize()
{
	Super::PreSerialize();

	SerializedIndexType = GetPin(IndexPin).GetType();
}

void FVoxelNode_Select::PostSerialize()
{
	GetPin(IndexPin).SetType(SerializedIndexType);
	FixupValuePins();

	Super::PostSerialize();
}

void FVoxelNode_Select::FixupValuePins()
{
	for (const FVoxelPinRef& Pin : ValuePins)
	{
		RemovePin(Pin);
	}
	ValuePins.Reset();

	const FVoxelPinType IndexType = GetPin(IndexPin).GetType();

	if (IndexType.IsWildcard())
	{
		return;
	}

	if (IndexType.Is<bool>() ||
		IndexType.Is<FVoxelBoolBuffer>())
	{
		ValuePins.Add(CreateInputPin(FVoxelPinType::MakeWildcard(), "False", {}));
		ValuePins.Add(CreateInputPin(FVoxelPinType::MakeWildcard(), "True", {}));
	}
	else if (
		IndexType.Is<int32>() ||
		IndexType.Is<FVoxelInt32Buffer>())
	{
		for (int32 Index = 0; Index < NumIntegerOptions; Index++)
		{
			ValuePins.Add(CreateInputPin(
				FVoxelPinType::MakeWildcard(),
				FName("Option", Index + 1),
				VOXEL_PIN_METADATA(
					void,
					nullptr,
					DisplayName("Option " + FString::FromInt(Index)))));
		}
	}
	else
	{
		ensure(false);
	}

	for (FVoxelPinRef& ValuePin : ValuePins)
	{
		GetPin(ValuePin).SetType(GetPin(ResultPin).GetType());
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
FString FVoxelNode_Select::FDefinition::GetAddPinLabel() const
{
	return "Add Option";
}

FString FVoxelNode_Select::FDefinition::GetAddPinTooltip() const
{
	return "Adds a new option to the node";
}

FString FVoxelNode_Select::FDefinition::GetRemovePinTooltip() const
{
	return "Removes last option from the node";
}

bool FVoxelNode_Select::FDefinition::CanAddInputPin() const
{
	return Node.GetPin(Node.IndexPin).GetType().GetInnerType().Is<int32>();
}

void FVoxelNode_Select::FDefinition::AddInputPin()
{
	Node.NumIntegerOptions++;
	Node.FixupValuePins();
}

bool FVoxelNode_Select::FDefinition::CanRemoveInputPin() const
{
	return
		Node.GetPin(Node.IndexPin).GetType().GetInnerType().Is<int32>() &&
		Node.NumIntegerOptions > 2;
}

void FVoxelNode_Select::FDefinition::RemoveInputPin()
{
	Node.NumIntegerOptions--;
	Node.FixupValuePins();
}
#endif