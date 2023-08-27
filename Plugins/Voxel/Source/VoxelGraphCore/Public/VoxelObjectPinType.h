// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelObjectPinType.generated.h"

template<typename>
struct TIsVoxelObjectStruct;

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelObjectPinType
{
	GENERATED_BODY()

	FVoxelObjectPinType() = default;
	virtual ~FVoxelObjectPinType() = default;

	virtual UClass* GetClass() const VOXEL_PURE_VIRTUAL({});
	virtual UScriptStruct* GetStruct() const VOXEL_PURE_VIRTUAL({});

	virtual UObject* GetObject(FConstVoxelStructView Struct) const VOXEL_PURE_VIRTUAL({});
	virtual FVoxelInstancedStruct GetStruct(UObject* Object) const VOXEL_PURE_VIRTUAL({});

	static const TVoxelMap<const UScriptStruct*, const FVoxelObjectPinType*>& StructToPinType();
};

#define DECLARE_VOXEL_OBJECT_PIN_TYPE(Type) \
	template<> \
	struct TIsVoxelObjectStruct<Type> \
	{ \
		static constexpr bool Value = true; \
	};

#define DEFINE_VOXEL_OBJECT_PIN_TYPE(StructType, ObjectType) \
	void Dummy1() { checkStatic(std::is_same_v<VOXEL_THIS_TYPE, StructType ## PinType>); } \
	void Dummy2() { static_assert(TIsVoxelObjectStruct<StructType>::Value, "Object pin type not declared: use DECLARE_VOXEL_OBJECT_PIN_TYPE(YourType);"); } \
	virtual UClass* GetClass() const override { return ObjectType::StaticClass(); } \
	virtual UScriptStruct* GetStruct() const override { return StructType::StaticStruct(); } \
	UObject* GetObject(const FConstVoxelStructView Struct) const override \
	{ \
		if (!ensure(Struct.IsValid()) || \
			!ensure(Struct.IsA<StructType>())) \
		{ \
			return nullptr; \
		} \
		\
		ObjectType* Object = nullptr; \
		StructType StructCopy = Struct.Get<StructType>(); \
		Convert(true, Object, StructCopy); \
		return Object; \
	} \
	FVoxelInstancedStruct GetStruct(UObject* Object) const \
	{ \
		FVoxelInstancedStruct Struct = FVoxelInstancedStruct::Make<StructType>(); \
		if (!Object) \
		{ \
			return Struct; \
		} \
		ObjectType* TypedObject = Cast<ObjectType>(Object); \
		if (!ensure(TypedObject)) \
		{ \
			return Struct; \
		} \
		Convert(false, TypedObject, Struct.Get<StructType>()); \
		ensure(TypedObject == Object); \
		return Struct; \
	} \
	void Convert(const bool bSetObject, ObjectType*& Object, StructType& Struct) const