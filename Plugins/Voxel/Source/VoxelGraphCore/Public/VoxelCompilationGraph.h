// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelGraph.h"
#include "VoxelRuntimePinValue.h"

struct FVoxelTemplateNodeUtilities;

DECLARE_VOXEL_MEMORY_STAT(VOXELGRAPHCORE_API, STAT_VoxelCompileGraph, "Compile Graph");

BEGIN_VOXEL_NAMESPACE(Graph)

class FPin;
class FNode;
class FGraph;

template<typename IteratorType, typename Type>
struct TDereferencerIterator
{
	IteratorType Iterator;

	TDereferencerIterator(IteratorType&& Iterator)
		: Iterator(Iterator)
	{
	}

	TDereferencerIterator& operator++()
	{
		++Iterator;
		return *this;
	}
	explicit operator bool() const
	{
		return bool(Iterator);
	}
	Type& operator*() const
	{
		return **Iterator;
	}

	friend bool operator!=(const TDereferencerIterator& Lhs, const TDereferencerIterator& Rhs)
	{
		return Lhs.Iterator != Rhs.Iterator;
	}
};

template<typename T>
class TPinsView
{
public:
	TPinsView() = default;
	TPinsView(const TConstVoxelArrayView<FPin*> Pins)
		: Pins(Pins)
	{
	}
	template<typename AllocatorType>
	TPinsView(const TVoxelArray<FPin*, AllocatorType>& Pins)
		: Pins(Pins)
	{
	}

	T& operator[](int32 Index)
	{
		return *Pins[Index];
	}
	const T& operator[](int32 Index) const
	{
		return *Pins[Index];
	}

	int32 Num() const
	{
		return Pins.Num();
	}

	TArray<FPin*> Array() const
	{
		return TArray<FPin*>(Pins);
	}

	using FIterator = TDereferencerIterator<FPin* const*, T>;

	FIterator begin() const { return FIterator{ Pins.begin() }; }
	FIterator end() const { return FIterator{ Pins.end() }; }

private:
	TConstVoxelArrayView<FPin*> Pins;
};

template<typename T>
class TNodesView
{
public:
	TNodesView(const TVoxelSet<FNode*>& Nodes)
		: Nodes(Nodes)
	{
	}

	int32 Num() const
	{
		return Nodes.Num();
	}

	using FIterator = TDereferencerIterator<TVoxelSet<FNode*>::TRangedForConstIterator, T>;

	FIterator begin() const { return FIterator{ Nodes.begin() }; }
	FIterator end() const { return FIterator{ Nodes.end() }; }

private:
	const TVoxelSet<FNode*>& Nodes;
};

template<typename T>
class TNodesCopy
{
public:
	TNodesCopy(const TVoxelSet<FNode*>& Nodes)
		: Nodes(Nodes)
	{
	}

	using FIterator = TDereferencerIterator<TVoxelSet<FNode*>::TRangedForConstIterator, T>;

	FIterator begin() const { return FIterator{ Nodes.begin() }; }
	FIterator end() const { return FIterator{ Nodes.end() }; }

private:
	TVoxelSet<FNode*> Nodes;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

enum class EPinDirection : uint8
{
	Input,
	Output
};

class VOXELGRAPHCORE_API FPin
{
public:
	const FName Name;
	const FVoxelPinType Type;
	const EPinDirection Direction;
	FNode& Node;

	UE_NONCOPYABLE(FPin);

public:
	const FVoxelPinValue& GetDefaultValue() const;
	void SetDefaultValue(const FVoxelPinValue& NewDefaultValue);

private:
	FVoxelPinValue DefaultValue;

public:
	TPinsView<FPin> GetLinkedTo() const
	{
		return LinkedTo;
	}

	void MakeLinkTo(FPin& Other)
	{
		ensure(Direction == EPinDirection::Input || Type.CanBeCastedTo(Other.Type));
		ensure(Direction == EPinDirection::Output || Other.Type.CanBeCastedTo(Type));
		ensure(Direction != Other.Direction);
		ensure(!LinkedTo.Contains(&Other));
		LinkedTo.Add(&Other);
		Other.LinkedTo.Add(this);
	}
	bool IsLinkedTo(FPin& Other) const
	{
		ensure(LinkedTo.Contains(&Other) == Other.LinkedTo.Contains(this));
		return LinkedTo.Contains(&Other);
	}
	void TryMakeLinkTo(FPin& Other)
	{
		if (!IsLinkedTo(Other))
		{
			MakeLinkTo(Other);
		}
	}

	void BreakLinkTo(FPin& Other)
	{
		ensure(LinkedTo.Remove(&Other));
		ensure(Other.LinkedTo.Remove(this));
	}
	void BreakAllLinks()
	{
		for (FPin* Other : LinkedTo)
		{
			ensure(Other->LinkedTo.Remove(this));
		}
		LinkedTo.Reset();
	}

	void CopyInputPinTo(FPin& Target) const
	{
		ensure(Type == Target.Type);
		ensure(Direction == Target.Direction);
		ensure(Direction == EPinDirection::Input);

		for (FPin* Other : LinkedTo)
		{
			Other->MakeLinkTo(Target);
		}

		if (LinkedTo.Num() == 0)
		{
			Target.SetDefaultValue(GetDefaultValue());
		}
	}
	void CopyOutputPinTo(FPin& Target) const
	{
		ensure(Type == Target.Type);
		ensure(Direction == Target.Direction);
		ensure(Direction == EPinDirection::Output);

		for (FPin* Other : LinkedTo)
		{
			Other->MakeLinkTo(Target);
		}
	}

	void Check(const FGraph& Graph) const;

private:
	TVoxelArray<FPin*, TVoxelInlineAllocator<4>> LinkedTo;

	FPin(
		const FName Name,
		const FVoxelPinType& Type,
		const FVoxelPinValue& DefaultValue,
		const EPinDirection Direction,
		FNode& Node)
		: Name(Name)
		, Type(Type)
		, Direction(Direction)
		, Node(Node)
		, DefaultValue(DefaultValue)
	{
		ensure(Type.IsValid());
	}

	template<typename, int32>
	friend class ::TVoxelChunkedArray;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

enum class ENodeType : uint8
{
	Root,
	Struct,
	// TODO Remove
	Passthrough
};

class VOXELGRAPHCORE_API FNode
{
public:
	const ENodeType Type;
	const FVoxelGraphNodeRef NodeRef;

public:
	template<typename T>
	const T& GetVoxelNode() const
	{
		ensure(Type == ENodeType::Struct);
		return CastChecked<T>(*VoxelNode);
	}
	const FVoxelNode& GetVoxelNode() const
	{
		ensure(Type == ENodeType::Struct);
		return *VoxelNode;
	}

	void SetVoxelNode(const TSharedRef<const FVoxelNode>& Node);
	void CopyFrom(const FNode& Src);

private:
	TSharedPtr<const FVoxelNode> VoxelNode;

public:
	FPin& GetInputPin(const int32 Index) { return *InputPins[Index]; }
	FPin& GetOutputPin(const int32 Index) { return *OutputPins[Index]; }

	const FPin& GetInputPin(const int32 Index) const { return *InputPins[Index]; }
	const FPin& GetOutputPin(const int32 Index) const { return *OutputPins[Index]; }

public:
	TPinsView<FPin> GetPins() { return Pins; }
	TPinsView<const FPin> GetPins() const { return Pins; }

	TPinsView<FPin> GetInputPins() { return InputPins; }
	TPinsView<FPin> GetOutputPins() { return OutputPins; }

	TPinsView<const FPin> GetInputPins() const { return InputPins; }
	TPinsView<const FPin> GetOutputPins() const { return OutputPins; }

public:
	FPin* FindInput(const FName Name);
	FPin* FindOutput(const FName Name);

	FPin& FindInputChecked(const FName Name);
	FPin& FindOutputChecked(const FName Name);

	const FPin* FindInput(const FName Name) const
	{
		return ConstCast(this)->FindInput(Name);
	}
	const FPin* FindOutput(const FName Name) const
	{
		return ConstCast(this)->FindOutput(Name);
	}

	const FPin& FindInputChecked(const FName Name) const
	{
		return ConstCast(this)->FindInputChecked(Name);
	}
	const FPin& FindOutputChecked(const FName Name) const
	{
		return ConstCast(this)->FindOutputChecked(Name);
	}

public:
	FPin& NewInputPin(FName Name, const FVoxelPinType& PinType, const FVoxelPinValue& DefaultValue = {});
	FPin& NewOutputPin(FName Name, const FVoxelPinType& PinType);

	void Check();
	void BreakAllLinks();

private:
	FNode(const ENodeType Type, const FVoxelGraphNodeRef& NodeRef, FGraph& Graph)
		: Type(Type)
		, NodeRef(NodeRef)
		, Graph(Graph)
	{
		ensure(!NodeRef.NodeId.IsNone());
		PinsMap.Reserve(NumInlinePins);
	}
	UE_NONCOPYABLE(FNode);

	FGraph& Graph;

	static constexpr int32 NumInlinePins = 8;

	TVoxelArray<FPin*, TVoxelInlineAllocator<NumInlinePins>> Pins;
	TVoxelAddOnlyMap<FName, FPin*, TVoxelAddOnlyMapArrayType<TVoxelInlineAllocator<NumInlinePins>>> PinsMap;

	TVoxelArray<FPin*, TVoxelInlineAllocator<NumInlinePins / 2>> InputPins;
	TVoxelArray<FPin*, TVoxelInlineAllocator<NumInlinePins / 2>> OutputPins;

	template<typename, int32>
	friend class ::TVoxelChunkedArray;
	friend FVoxelTemplateNodeUtilities;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELGRAPHCORE_API FGraph
{
public:
	FGraph();
	UE_NONCOPYABLE(FGraph);

	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelCompileGraph);

	int64 GetAllocatedSize() const;

public:
	TNodesView<FNode> GetNodes()
	{
		return Nodes;
	}
	TNodesView<const FNode> GetNodes() const
	{
		return Nodes;
	}

	TNodesCopy<FNode> GetNodesCopy()
	{
		return Nodes;
	}
	TNodesCopy<const FNode> GetNodesCopy() const
	{
		return Nodes;
	}

	TArray<FNode*> GetNodesArray()
	{
		return Nodes.Array();
	}
	TArray<const FNode*> GetNodesArray() const
	{
		return ReinterpretCastArray<const FNode*>(Nodes.Array());
	}

	FNode& NewNode(ENodeType Type, const FVoxelGraphNodeRef& NodeRef);

	template<typename PredicateType>
	void RemoveNodes(PredicateType Predicate)
	{
		for (auto It = Nodes.CreateIterator(); It; ++It)
		{
			FNode& Node = **It;
			if (!Predicate(Node))
			{
				continue;
			}

			Node.BreakAllLinks();
			It.RemoveCurrent();
		}
	}
	void RemoveNode(FNode& Node)
	{
		Node.BreakAllLinks();
		ensure(Nodes.Remove(&Node));
	}

	void Check();
	TSharedRef<FGraph> Clone() const;

	bool Identical(const FGraph& Other, FString* OutDiff = nullptr) const;

private:
	TVoxelSet<FNode*> Nodes;

	TVoxelChunkedArray<FPin, sizeof(FPin) * 64> PinAllocations;
	TVoxelChunkedArray<FNode, sizeof(FNode) * 16> NodeAllocations;

	FPin& NewPin(
		FName Name,
		const FVoxelPinType& Type,
		const FVoxelPinValue& DefaultValue,
		EPinDirection Direction,
		FNode& Node);

	friend class FPin;
	friend class FNode;
};

END_VOXEL_NAMESPACE(Graph)