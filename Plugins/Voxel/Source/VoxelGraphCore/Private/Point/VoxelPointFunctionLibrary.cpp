// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "Point/VoxelPointFunctionLibrary.h"
#include "Point/VoxelPointSet.h"
#include "Point/VoxelPointStorage.h"
#include "Point/VoxelPointStorageData.h"
#include "Point/VoxelPointHandleProvider.h"
#include "VoxelRuntime.h"

EVoxelSuccess UVoxelPointFunctionLibrary::MakePointHandle(const FHitResult& HitResult, FVoxelPointHandle& Handle)
{
	VOXEL_FUNCTION_COUNTER();

	Handle = {};

	if (!HitResult.bBlockingHit)
	{
		return EVoxelSuccess::Failed;
	}

	const UPrimitiveComponent* Component = HitResult.GetComponent();
	if (!Component)
	{
		return EVoxelSuccess::Failed;
	}

	const IVoxelPointHandleProvider* Provider = Cast<IVoxelPointHandleProvider>(Component);
	if (!Provider)
	{
		return EVoxelSuccess::Failed;
	}

	if (!Provider->TryGetPointHandle(HitResult.Item, Handle))
	{
		return EVoxelSuccess::Failed;
	}

	return EVoxelSuccess::Succeeded;
}

EVoxelSuccess UVoxelPointFunctionLibrary::GetPointTransform(
	const FVoxelPointHandle& Handle,
	FTransform& Transform)
{
	VOXEL_FUNCTION_COUNTER();

	Transform = FTransform::Identity;

	if (!Handle.IsValid())
	{
		VOXEL_MESSAGE(Error, "Invalid handle");
		return EVoxelSuccess::Failed;
	}

	FString Error;

	const TSharedPtr<FVoxelRuntime> Runtime = Handle.GetRuntime(&Error);
	if (!Runtime)
	{
		VOXEL_MESSAGE(Error, "Failed to resolve runtime: {0}", Error);
		return EVoxelSuccess::Failed;
	}

	TVoxelMap<FName, FVoxelPinValue> Attributes;
	if (!Handle.GetAttributes(Attributes, &Error))
	{
		VOXEL_MESSAGE(Error, "Failed to get attributes: {0}", Error);
		return EVoxelSuccess::Failed;
	}

	const FVoxelPinValue Position = Attributes.FindRef(FVoxelPointAttributes::Position);
	const FVoxelPinValue Rotation = Attributes.FindRef(FVoxelPointAttributes::Rotation);
	const FVoxelPinValue Scale = Attributes.FindRef(FVoxelPointAttributes::Scale);

	if (ensure(Position.Is<FVector>()))
	{
		Transform.SetLocation(Position.Get<FVector>());
	}
	if (ensure(Rotation.Is<FQuat>()))
	{
		Transform.SetRotation(Rotation.Get<FQuat>());
	}
	if (ensure(Scale.Is<FVector>()))
	{
		Transform.SetScale3D(Scale.Get<FVector>());
	}

	Transform *= FTransform(Runtime->GetLocalToWorld().Get_NoDependency());
	return EVoxelSuccess::Succeeded;
}

bool UVoxelPointFunctionLibrary::HasPointAttribute(
	const FVoxelPointHandle& Handle,
	const FName Name)
{
	FString Error;
	const FVoxelPinValue Value = GetPointAttribute(
		Handle,
		Name,
		&Error);

	return Value.IsValid();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UVoxelPointFunctionLibrary::SetPointAttribute(
	const FVoxelPointHandle& Handle,
	const FName Name,
	const FVoxelPinType& Type,
	const FVoxelPinValue& Value,
	FString* OutError)
{
	VOXEL_FUNCTION_COUNTER();

	if (!Handle.IsValid())
	{
		if (OutError)
		{
			*OutError = "Invalid handle";
		}
		return false;
	}

	const TSharedPtr<FVoxelRuntime> Runtime = Handle.GetRuntime(OutError);
	if (!Runtime)
	{
		return false;
	}

	const TSharedPtr<const FVoxelRuntimeParameter_PointStorage> Parameter = Runtime->FindParameter<FVoxelRuntimeParameter_PointStorage>();
	if (!Parameter ||
		!ensure(Parameter->Data))
	{
		if (OutError)
		{
			*OutError = "Runtime has no point storage";
		}
		return false;
	}

	return Parameter->Data->SetPointAttribute(
		Handle,
		Name,
		Type,
		Value,
		OutError);
}

DEFINE_FUNCTION(UVoxelPointFunctionLibrary::execK2_SetPointAttribute)
{
	P_GET_STRUCT(FVoxelPointHandle, Handle);
	P_GET_STRUCT(FName, Name);
	P_GET_STRUCT(FVoxelPinType, Type);

	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;

	Stack.StepCompiledIn<FProperty>(nullptr);

	const FProperty* Property = Stack.MostRecentProperty;
	const void* PropertyAddress = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!ensure(Property))
	{
		VOXEL_MESSAGE(Error, "Failed to resolve the Value parameter");
		return;
	}

	P_NATIVE_BEGIN

	FString Error;
	if (!SetPointAttribute(
		Handle,
		Name,
		Type,
		FVoxelPinValue::MakeFromProperty(*Property, PropertyAddress),
		&Error))
	{
		VOXEL_MESSAGE(Error, "SetPointAttribute failed: {0}", Error);
	}

	P_NATIVE_END
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_FUNCTION(UVoxelPointFunctionLibrary::execK2_GetPointAttribute)
{
	P_GET_STRUCT(FVoxelPointHandle, Handle);
	P_GET_STRUCT(FName, Name);
	P_GET_STRUCT(FVoxelPinType, InType);
	P_GET_ENUM_REF(EVoxelSuccess, Success);

	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;

	Stack.StepCompiledIn<FProperty>(nullptr);

	FProperty* Property = Stack.MostRecentProperty;
	void* PropertyAddress = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!ensure(Property))
	{
		VOXEL_MESSAGE(Error, "Failed to resolve the Value parameter");
		return;
	}

	P_NATIVE_BEGIN

	FString Error;
	const FVoxelPinValue Value = GetPointAttribute(
		Handle,
		Name,
		&Error);

	if (!Value.IsValid())
	{
		VOXEL_MESSAGE(Error, "GetPointAttribute failed: {0}", Error);
		Success = EVoxelSuccess::Failed;
		return;
	}

	const FVoxelPinType Type(*Property);
	if (Value.GetType() != Type)
	{
		VOXEL_MESSAGE(Error, "Type mismatch: querying with type {0}, but attriute has type {1}", Type.ToString(), Value.GetType().ToString());
		Success = EVoxelSuccess::Failed;
		return;
	}

	Value.ExportToProperty(*Property, PropertyAddress);
	Success = EVoxelSuccess::Succeeded;

	P_NATIVE_END
}

FVoxelPinValue UVoxelPointFunctionLibrary::GetPointAttribute(
	const FVoxelPointHandle& Handle,
	const FName Name,
	FString* OutError)
{
	VOXEL_FUNCTION_COUNTER();

	if (!Handle.IsValid())
	{
		if (OutError)
		{
			*OutError = "Invalid handle";
		}
		return {};
	}

	const TSharedPtr<FVoxelRuntime> Runtime = Handle.GetRuntime(OutError);
	if (!Runtime)
	{
		return {};
	}

	TVoxelMap<FName, FVoxelPinValue> Attributes;
	if (!Handle.GetAttributes(Attributes, OutError))
	{
		return {};
	}

	if (!Attributes.Contains(Name))
	{
		if (OutError)
		{
			TArray<FString> ValidNames;
			for (const auto& It : Attributes)
			{
				ValidNames.Add(It.Key.ToString());
			}

			*OutError = "No attribute named " + Name.ToString() + ". Valid names: " + FString::Join(ValidNames, TEXT(", "));
		}

		return {};
	}

	return Attributes[Name];
}