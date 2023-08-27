// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelExec.h"
#include "VoxelNode.h"
#include "VoxelExecNode.generated.h"

struct FVoxelSpawnable;
struct FVoxelMergeSpawnableRef;
class FVoxelRuntime;
class FVoxelExecNodeRuntime;

USTRUCT(Category = "Exec Nodes", meta = (Abstract, NodeColor = "Red", NodeIconColor = "White", ShowInRootShortList))
struct VOXELGRAPHCORE_API FVoxelExecNode : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	// If false, the node will never be executed
	VOXEL_INPUT_PIN(bool, EnableNode, true, VirtualPin);
	// If not connected, will be executed automatically
	VOXEL_OUTPUT_PIN(FVoxelExec, Exec, OptionalPin);

public:
	TSharedPtr<FVoxelExecNodeRuntime> CreateSharedExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const;

protected:
	virtual TVoxelUniquePtr<FVoxelExecNodeRuntime> CreateExecRuntime(const TSharedRef<const FVoxelExecNode>& SharedThis) const VOXEL_PURE_VIRTUAL({});
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class IVoxelExecNodeRuntimeInterface
{
public:
	IVoxelExecNodeRuntimeInterface() = default;
	virtual ~IVoxelExecNodeRuntimeInterface() = default;

	virtual const FVoxelQueryContext* GetContextPtr() { return nullptr; }

	virtual void Tick(FVoxelRuntime& Runtime) {}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
	virtual FVoxelOptionalBox GetBounds() const { return {}; }
	virtual TVoxelFutureValue<FVoxelSpawnable> GenerateSpawnable(const FVoxelMergeSpawnableRef& Ref) { return {}; }
};

class VOXELGRAPHCORE_API FVoxelExecNodeRuntime
	: public IVoxelExecNodeRuntimeInterface
	, public IVoxelNodeInterface
	, public FVoxelNodeAliases
	, public TSharedFromThis<FVoxelExecNodeRuntime>
	, public TVoxelRuntimeInfo<FVoxelExecNodeRuntime>
{
public:
	using NodeType = FVoxelExecNode;
	const TSharedRef<const FVoxelExecNode> NodeRef;

	explicit FVoxelExecNodeRuntime(const TSharedRef<const FVoxelExecNode>& NodeRef)
		: NodeRef(NodeRef)
	{
	}
	virtual ~FVoxelExecNodeRuntime() override;

	VOXEL_COUNT_INSTANCES();

	//~ Begin IVoxelNodeInterface Interface
	FORCEINLINE virtual const FVoxelGraphNodeRef& GetNodeRef() const final override
	{
		return NodeRef->GetNodeRef();
	}
	//~ End IVoxelNodeInterface Interface

	// True before Destroy is called
	bool IsDestroyed() const
	{
		return bIsDestroyed;
	}

	const FVoxelNodeRuntime& GetNodeRuntime() const
	{
		return NodeRef->GetNodeRuntime();
	}
	TSharedRef<FVoxelQueryContext> GetContext() const
	{
		return PrivateContext.ToSharedRef();
	}
	const TSharedRef<const FVoxelRuntimeInfo>& GetRuntimeInfo() const
	{
		return PrivateContext->RuntimeInfo;
	}
	FORCEINLINE const FVoxelRuntimeInfo& GetRuntimeInfoRef() const
	{
		checkVoxelSlow(PrivateContext);
		return *PrivateContext.Get()->RuntimeInfo;
	}

	TSharedPtr<FVoxelRuntime> GetRuntime() const;
	USceneComponent* GetRootComponent() const;

	virtual UScriptStruct* GetNodeType() const
	{
		return StaticStructFast<NodeType>();
	}
	virtual const FVoxelQueryContext* GetContextPtr() final override
	{
		return &GetContext().Get();
	}

public:
	void CallCreate(const TSharedRef<FVoxelQueryContext>& Context);

	virtual void PreCreate() {}
	virtual void Create() {}
	virtual void Destroy() {}

private:
	bool bIsCreated = false;
	bool bIsDestroyed = false;
	TSharedPtr<FVoxelQueryContext> PrivateContext;

	void CallDestroy();

	friend FVoxelExecNode;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename InNodeType>
class TVoxelExecNodeRuntime : public FVoxelExecNodeRuntime
{
public:
	using NodeType = InNodeType;
	using Super = TVoxelExecNodeRuntime;

	const NodeType& Node;

	explicit TVoxelExecNodeRuntime(const TSharedRef<const FVoxelExecNode>& NodeRef)
		: FVoxelExecNodeRuntime(NodeRef)
		, Node(CastChecked<NodeType>(*NodeRef))
	{
		checkStatic(TIsDerivedFrom<NodeType, FVoxelExecNode>::Value);
	}

	virtual UScriptStruct* GetNodeType() const override
	{
		return StaticStructFast<NodeType>();
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define INTERNAL_DECLARE_VOXEL_PIN_VALUES_DECLARE_TYPES(Name) \
	using Name ## _RawType = decltype(NodeType::Name ## Pin)::Type; \
	using Name ## _Type = TChooseClass<VoxelPassByValue<Name ## _RawType>, Name ## _RawType, TSharedPtr<const Name ## _RawType>>::Result;

#define INTERNAL_DECLARE_VOXEL_PIN_VALUES_DECLARE_VALUES(Name) \
	Name ## _Type Name = FVoxelUtilities::MakeSafe<Name ## _Type>();

#define INTERNAL_DECLARE_VOXEL_PIN_VALUES_DECLARE_DYNAMIC_VALUES(Name) \
	TVoxelDynamicValue<Name ## _RawType> Value_ ## Name; \
	bool bIsSet_ ## Name = false;

#define INTERNAL_DECLARE_VOXEL_PIN_VALUES_MAKE_VALUE(Name) \
	ensure(!Value_ ## Name.IsValid()); \
	Value_ ## Name = Node->GetNodeRuntime().MakeDynamicValueFactory(Node->Name ## Pin) \
		.Thread(EVoxelTaskThread::GameThread) \
		.Compute(Context, Parameters); \
	\
	Value_ ## Name.OnChanged(MakeWeakPtrLambda(Owner, [this, CallbackFunction](const Name ## _Type& NewValue) \
	{ \
		VOXEL_SCOPE_LOCK(CriticalSection); \
		PinValues.Name = NewValue; \
		bIsSet_ ## Name = true; \
		CallbackFunction(); \
	}));

#define INTERNAL_DECLARE_VOXEL_PIN_VALUES_IS_SET(Name) && bIsSet_ ## Name

#define DECLARE_VOXEL_PIN_VALUES(...) \
	struct FPinValues \
	{ \
		VOXEL_FOREACH(INTERNAL_DECLARE_VOXEL_PIN_VALUES_DECLARE_TYPES, __VA_ARGS__); \
		VOXEL_FOREACH(INTERNAL_DECLARE_VOXEL_PIN_VALUES_DECLARE_VALUES, __VA_ARGS__); \
	}; \
	struct FPinValuesProvider \
	{ \
		VOXEL_FOREACH(INTERNAL_DECLARE_VOXEL_PIN_VALUES_DECLARE_TYPES, __VA_ARGS__); \
		\
		template<typename T> \
		void Compute( \
			T* Owner, \
			const TSharedRef<const NodeType>& Node, \
			const TSharedRef<FVoxelQueryContext>& Context, \
			const TSharedRef<const FVoxelQueryParameters>& Parameters, \
			TFunction<void(const FPinValues& PinValues)> OnChanged) \
		{ \
			const TFunction<void()> CallbackFunction = [this, Owner, OnChanged = MoveTemp(OnChanged)] \
			{ \
				if (!(true VOXEL_FOREACH(INTERNAL_DECLARE_VOXEL_PIN_VALUES_IS_SET, __VA_ARGS__))) \
				{ \
					return; \
				} \
				if (!ensureVoxelSlow(!Owner->IsDestroyed())) \
				{ \
					return; \
				} \
				\
				OnChanged(PinValues); \
			}; \
			\
			VOXEL_FOREACH(INTERNAL_DECLARE_VOXEL_PIN_VALUES_MAKE_VALUE, __VA_ARGS__); \
		} \
		template<typename T> \
		void Compute( \
			T* Owner, \
			const TSharedRef<const FVoxelQueryParameters>& Parameters, \
			TFunction<void(const FPinValues& PinValues)> OnChanged) \
		{ \
			Compute( \
				Owner, \
				CastChecked<NodeType>(Owner->NodeRef), \
				Owner->GetContext(), \
				Parameters, \
				MoveTemp(OnChanged)); \
		} \
		template<typename T> \
		void Compute( \
			T* Owner, \
			TFunction<void(const FPinValues& PinValues)> OnChanged) \
		{ \
			Compute( \
				Owner, \
				MakeVoxelShared<FVoxelQueryParameters>(), \
				MoveTemp(OnChanged)); \
		} \
	private: \
		FVoxelFastCriticalSection CriticalSection; \
		FPinValues PinValues; \
		VOXEL_FOREACH(INTERNAL_DECLARE_VOXEL_PIN_VALUES_DECLARE_DYNAMIC_VALUES, __VA_ARGS__); \
	}; \
	FPinValuesProvider PinValuesProvider;