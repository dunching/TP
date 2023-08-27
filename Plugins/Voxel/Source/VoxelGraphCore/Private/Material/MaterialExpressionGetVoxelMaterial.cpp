// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "Material/MaterialExpressionGetVoxelMaterial.h"
#include "MaterialCompiler.h"
#include "Engine/Texture2D.h"
#include "Material/VoxelMaterialDefinition.h"

DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelMaterialTextureMemory);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionGetVoxelMaterial_Base::UMaterialExpressionGetVoxelMaterial_Base()
{
	bHidePreviewWindow = true;
}

const UMaterialExpressionGetVoxelMaterial_Base* UMaterialExpressionGetVoxelMaterial_Base::GetTemplate(const FVoxelPinType& Type)
{
	static TVoxelAddOnlyMap<FVoxelPinType, TSubclassOf<UMaterialExpressionGetVoxelMaterial_Base>> TypeToGetVoxelMaterialClass;
	if (TypeToGetVoxelMaterialClass.Num() == 0)
	{
		for (const TSubclassOf<UMaterialExpressionGetVoxelMaterial_Base> Class : GetDerivedClasses<UMaterialExpressionGetVoxelMaterial_Base>())
		{
			if (Class->HasAnyClassFlags(CLASS_Abstract))
			{
				continue;
			}
			TypeToGetVoxelMaterialClass.Add_CheckNew(Class.GetDefaultObject()->GetVoxelParameterType(), Class);
		}
	}
	const TSubclassOf<UMaterialExpressionGetVoxelMaterial_Base> Class = TypeToGetVoxelMaterialClass.FindRef(Type);
	if (!Class)
	{
		return nullptr;
	}
	return Class.GetDefaultObject();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UObject* UMaterialExpressionGetVoxelMaterial_Base::GetReferencedTexture() const
{
	return FVoxelTextureUtilities::GetDefaultTexture2D();
}

#if WITH_EDITOR
int32 UMaterialExpressionGetVoxelMaterial_Base::Compile(FMaterialCompiler* Compiler, const int32 OutputIndex)
{
	TVoxelInstancedStruct<FVoxelMaterialParameterData> ParameterData;
	if (MaterialDefinition)
	{
		const FVoxelParameter* Parameter = MaterialDefinition->Parameters.FindByKey(ParameterName);
		if (!Parameter)
		{
			return Compiler->Errorf(
				TEXT("No parameter named %s on %s"),
				*ParameterName.ToString(),
				*MaterialDefinition->GetPathName());
		}

		if (Parameter->Type != GetVoxelParameterType())
		{
			return Compiler->Errorf(
				TEXT("Parameter has type %s but type %s was expected"),
				*Parameter->Type.ToString(),
				*GetVoxelParameterType().ToString());
		}

		ParameterData = MaterialDefinition->GuidToParameterData.FindRef(Parameter->Guid);
	}

	if (!ensure(FVoxelTextureUtilities::GetDefaultTexture2D()))
	{
		return -1;
	}

	const FString Name = "VOXELPARAM_" + ParameterName.ToString();
	const int32 MaterialIdValue = MaterialId.Compile(Compiler);
	const int32 PreviewMaterialIdValue = Compiler->ScalarParameter("PreviewMaterialId", 0.f);

	UMaterialExpressionCustom* Custom = NewObject<UMaterialExpressionCustom>();
	Custom->Inputs.Empty();

	TArray<int32> Inputs;
	CompileVoxel(
		*Compiler,
		*Custom,
		ParameterData.GetPtr(),
		FName(Name),
		MaterialIdValue,
		PreviewMaterialIdValue,
		Inputs);

	if (!ensure(!Inputs.Contains(-1)))
	{
		return -1;
	}

	return Compiler->CustomExpression(Custom, 0, Inputs);
}

void UMaterialExpressionGetVoxelMaterial_Base::GetCaption(TArray<FString>& OutCaptions) const
{
	FString Name = GetClass()->GetDisplayNameText().ToString();
	Name.RemoveFromStart("Material Expression ");
	OutCaptions.Add(Name);
}
#endif