// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "FunctionLibrary/VoxelParameterFunctionLibrary.h"
#include "VoxelParameterView.h"
#include "VoxelParameterContainer.h"

void UVoxelParameterFunctionLibrary::K2_GetVoxelParameter(
	UVoxelParameterContainer* ParameterContainer,
	FName Name,
	int32& Value)
{
	check(false);
}

void UVoxelParameterFunctionLibrary::GetVoxelParameterImpl(
	UVoxelParameterContainer* ParameterContainer,
	const FName Name,
	void* OutData,
	const FProperty* PropertyForTypeCheck)
{
	if (!ParameterContainer)
	{
		VOXEL_MESSAGE(Error, "ParameterContainer is null");
		return;
	}

	const TSharedPtr<IVoxelParameterRootView> ParameterRootView = ParameterContainer->GetParameterView();
	if (!ensure(ParameterRootView))
	{
		return;
	}

	const IVoxelParameterView* ParameterView = ParameterRootView->FindByName(Name);
	if (!ParameterView)
	{
		VOXEL_MESSAGE(Error, "Failed to find specified parameter ({0}). Valid parameters: {1}", Name, ParameterRootView->GetValidParameters());
		return;
	}

	const FVoxelPinValue Value = ParameterView->GetValue();
	const FVoxelPinType OutType = FVoxelPinType(*PropertyForTypeCheck);
	if (!Value.CanBeCastedTo(OutType))
	{
		VOXEL_MESSAGE(Error, "Invalid parameter type ({0}). Required type: {1}", OutType.ToString(), Value.GetType().ToString());
		return;
	}

	Value.ExportToProperty(*PropertyForTypeCheck, OutData);
}

DEFINE_FUNCTION(UVoxelParameterFunctionLibrary::execK2_GetVoxelParameter)
{
	P_GET_OBJECT(UVoxelParameterContainer, ParameterContainer);
	P_GET_STRUCT(FName, Name);

	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;

	Stack.StepCompiledIn<FProperty>(nullptr);

	P_FINISH;

	if (!ensure(Stack.MostRecentProperty))
	{
		VOXEL_MESSAGE(Error, "Failed to resolve the Value parameter");
		return;
	}

	P_NATIVE_BEGIN
	P_THIS->GetVoxelParameterImpl(ParameterContainer, Name, Stack.MostRecentPropertyAddress, Stack.MostRecentProperty);
	P_NATIVE_END
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelParameterFunctionLibrary::K2_SetVoxelParameter(
	UVoxelParameterContainer* ParameterContainer,
	FName Name,
	const int32& Value,
	int32& OutValue)
{
	check(false);
}

void UVoxelParameterFunctionLibrary::SetVoxelParameterImpl(
	UVoxelParameterContainer* ParameterContainer,
	const FName Name,
	const void* ValuePtr,
	const FProperty* ValueProperty)
{
	if (!ParameterContainer)
	{
		VOXEL_MESSAGE(Error, "ParameterContainer is null");
		return;
	}

	const FVoxelPinValue Value = FVoxelPinValue::MakeFromProperty(*ValueProperty, ValuePtr);

	FString Error;
	if (!ParameterContainer->Set(Name, Value, &Error))
	{
		VOXEL_MESSAGE(Error, "{0}", Error);
	}
}

DEFINE_FUNCTION(UVoxelParameterFunctionLibrary::execK2_SetVoxelParameter)
{
	P_GET_OBJECT(UVoxelParameterContainer, ParameterContainer);
	P_GET_STRUCT(FName, Name);

	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;

	Stack.StepCompiledIn<FProperty>(nullptr);

	const FProperty* Property = Stack.MostRecentProperty;
	const void* PropertyAddress = Stack.MostRecentPropertyAddress;

	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;

	Stack.StepCompiledIn<FProperty>(nullptr);

	P_FINISH;

	if (!ensure(Property))
	{
		VOXEL_MESSAGE(Error, "Failed to resolve the Value parameter");
		return;
	}

	P_NATIVE_BEGIN
	P_THIS->SetVoxelParameterImpl(ParameterContainer, Name, PropertyAddress, Property);
	P_NATIVE_END
}