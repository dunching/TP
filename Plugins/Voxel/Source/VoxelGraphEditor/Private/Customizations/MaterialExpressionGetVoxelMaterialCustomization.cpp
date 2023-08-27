// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "Material/VoxelMaterialDefinition.h"
#include "Material/MaterialExpressionGetVoxelMaterial.h"

VOXEL_CUSTOMIZE_CLASS(UMaterialExpressionGetVoxelMaterial_Base)(IDetailLayoutBuilder& DetailLayout)
{
	VOXEL_FUNCTION_COUNTER_LLM();

	const TSharedRef<IPropertyHandle> MaterialDefinitionHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(UMaterialExpressionGetVoxelMaterial_Base, MaterialDefinition));
	const TSharedRef<IPropertyHandle> ParameterNameHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(UMaterialExpressionGetVoxelMaterial_Base, ParameterName));

	MaterialDefinitionHandle->SetOnPropertyValueChanged(FVoxelEditorUtilities::MakeRefreshDelegate(MaterialDefinitionHandle, DetailLayout));

	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = DetailLayout.GetSelectedObjects();

	TSet<FVoxelPinType> ValidTypes;
	TSet<UVoxelMaterialDefinition*> MaterialDefinitions;
	for (const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjects)
	{
		const UMaterialExpressionGetVoxelMaterial_Base* Expression = Cast<UMaterialExpressionGetVoxelMaterial_Base>(SelectedObject.Get());
		if (!ensure(Expression))
		{
			continue;
		}

		ValidTypes.Add(Expression->GetVoxelParameterType());
		MaterialDefinitions.Add(Expression->MaterialDefinition);
	}
	if (ValidTypes.Num() != 1 ||
		MaterialDefinitions.Num() != 1)
	{
		return;
	}

	const FVoxelPinType ValidType = ValidTypes.Array()[0];
	const TWeakObjectPtr<UVoxelMaterialDefinition> MaterialDefinition = MaterialDefinitions.Array()[0];
	if (!MaterialDefinition.IsValid())
	{
		return;
	}

	FString CurrentValue;
	if (ParameterNameHandle->GetValue(CurrentValue) != FPropertyAccess::Success)
	{
		return;
	}

	MaterialDefinitionHandle->MarkHiddenByCustomization();
	ParameterNameHandle->MarkHiddenByCustomization();

	IDetailCategoryBuilder& Category = DetailLayout.EditCategory(STATIC_FNAME("Config"));

	Category.AddProperty(MaterialDefinitionHandle);

	Category.AddCustomRow(INVTEXT("Name"))
	.NameContent()
	[
		ParameterNameHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SBox)
		.MinDesiredWidth(125.f)
		[
			SNew(SVoxelDetailComboBox<FName>)
			.RefreshDelegate(ParameterNameHandle, DetailLayout)
			.Options_Lambda([=]() -> TArray<FName>
			{
				if (!ensure(MaterialDefinition.IsValid()))
				{
					return {};
				}

				TArray<FName> Names;
				for (const FVoxelParameter& Parameter : MaterialDefinition->Parameters)
				{
					if (Parameter.Type != ValidType)
					{
						continue;
					}

					Names.Add(Parameter.Name);
				}
				return Names;
			})
			.CurrentOption(FName(CurrentValue))
			.OptionText(MakeLambdaDelegate([](const FName Option)
			{
				return Option.ToString();
			}))
			.OnSelection_Lambda([ParameterNameHandle](const FName NewValue)
			{
				ParameterNameHandle->SetValue(NewValue);
			})
		]
	];
}