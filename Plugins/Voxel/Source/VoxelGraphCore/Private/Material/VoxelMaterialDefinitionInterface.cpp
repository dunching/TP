// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "Material/VoxelMaterialDefinitionInterface.h"
#include "Material/VoxelMaterialDefinition.h"
#include "Material/VoxelMaterialDefinitionInstance.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

FVoxelMaterialDefinitionManager* GVoxelMaterialDefinitionManager = nullptr;

VOXEL_RUN_ON_STARTUP_GAME(RegisterVoxelMaterialDefinitionManager)
{
	GVoxelMaterialDefinitionManager = new FVoxelMaterialDefinitionManager();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UMaterialInterface* UVoxelMaterialDefinitionInterface::GetPreviewMaterial()
{
	const UVoxelMaterialDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}
	if (!PrivatePreviewMaterial ||
		PrivatePreviewMaterialParent != Definition->PreviewMaterial)
	{
		PrivatePreviewMaterial = FVoxelMaterialRef::MakeInstance(Definition->PreviewMaterial);
		PrivatePreviewMaterialParent = Definition->PreviewMaterial;

		PrivatePreviewMaterial->SetScalarParameter_GameThread(
			"PreviewMaterialId",
			GVoxelMaterialDefinitionManager->Register_GameThread(*this).Index);

		PrivatePreviewMaterial->SetDynamicParameter_GameThread(
			"GVoxelMaterialDefinitionManager",
			GVoxelMaterialDefinitionManager->DynamicParameter);
	}

	return PrivatePreviewMaterial->GetMaterial();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMaterialDefinitionDynamicMaterialParameter::Apply(const FName Name, UMaterialInstanceDynamic& Instance) const
{
	GVoxelMaterialDefinitionManager->SetAllParameters(Instance);
}

void FVoxelMaterialDefinitionDynamicMaterialParameter::AddOnChanged(const FSimpleDelegate & OnChanged)
{
	OnChangedMulticast.Add(OnChanged);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelMaterialDefinitionManager::FVoxelMaterialDefinitionManager()
{
	Materials.Add(nullptr);
}

FVoxelMaterialDefinitionRef FVoxelMaterialDefinitionManager::Register_GameThread(UVoxelMaterialDefinitionInterface& Material)
{
	check(IsInGameThread());

	if (const FVoxelMaterialDefinitionRef* MaterialRef = MaterialRefs.Find(&Material))
	{
		return *MaterialRef;
	}

	VOXEL_FUNCTION_COUNTER();

	const FVoxelMaterialDefinitionRef MaterialRef{ uint16(Materials.Num()) };

	if (Materials.Num() == GVoxelMaterialDefinitionMax - 1)
	{
		VOXEL_MESSAGE(Error, "Max number of voxel materials reached");
	}
	else
	{
		ensure(Materials.Num() < GVoxelMaterialDefinitionMax);
		Materials.Add(&Material);
	}

	MaterialRefs.Add(&Material, MaterialRef);
	bMaterialRefreshQueued = true;

	if (UVoxelMaterialDefinition* MaterialDefinition = Cast<UVoxelMaterialDefinition>(&Material))
	{
		MaterialDefinitionsToRebuild.Add(MaterialDefinition);
	}
	if (const UVoxelMaterialDefinitionInstance* MaterialInstance = Cast<UVoxelMaterialDefinitionInstance>(&Material))
	{
		if (UVoxelMaterialDefinition* MaterialDefinition = MaterialInstance->GetDefinition())
		{
			MaterialDefinitionsToRebuild.Add(MaterialDefinition);
		}
	}

	return MaterialRef;
}

UVoxelMaterialDefinitionInterface* FVoxelMaterialDefinitionManager::GetMaterial_GameThread(const FVoxelMaterialDefinitionRef& Ref)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!Materials.IsValidIndex(Ref.Index))
	{
		return nullptr;
	}

	return Materials[Ref.Index];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMaterialDefinitionManager::Tick()
{
	VOXEL_FUNCTION_COUNTER();

	if (bMaterialRefreshQueued)
	{
		bMaterialRefreshQueued = false;
		CacheParameters();

		VOXEL_SCOPE_COUNTER("OnChangedMulticast");
		DynamicParameter->OnChangedMulticast.Broadcast();
	}

	const TSet<UVoxelMaterialDefinition*> MaterialDefinitionsToRebuildCopy = MoveTemp(MaterialDefinitionsToRebuild);
	check(MaterialDefinitionsToRebuild.Num() == 0);

	for (UVoxelMaterialDefinition* Definition : MaterialDefinitionsToRebuildCopy)
	{
		if (ensure(Definition))
		{
			Definition->RebuildTextures();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelMaterialDefinitionManager::GetReferencerName() const
{
	return "FVoxelMaterialDefinitionManager";
}

void FVoxelMaterialDefinitionManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	Collector.AddReferencedObjects(MaterialDefinitionsToRebuild);

	for (UVoxelMaterialDefinitionInterface*& Material : Materials)
	{
		Collector.AddReferencedObject(Material);
	}

	for (auto It = MaterialRefs.CreateIterator(); It; ++It)
	{
		Collector.AddReferencedObject(It.Key());

		if (!It.Key())
		{
			It.RemoveCurrent();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMaterialDefinitionManager::QueueMaterialRefresh()
{
	bMaterialRefreshQueued = true;
}

void FVoxelMaterialDefinitionManager::QueueRebuildTextures(UVoxelMaterialDefinition& Definition)
{
	MaterialDefinitionsToRebuild.Add(&Definition);
}

void FVoxelMaterialDefinitionManager::CacheParameters()
{
	VOXEL_FUNCTION_COUNTER();

	CachedParameters = MakeUnique<FVoxelMaterialParameterData::FCachedParameters>();

	for (UVoxelMaterialDefinitionInterface* Material : Materials)
	{
		UVoxelMaterialDefinition* MaterialDefinition = Cast<UVoxelMaterialDefinition>(Material);
		if (!MaterialDefinition)
		{
			continue;
		}

		TVoxelAddOnlyMap<FGuid, FName> GuidToParameterName;
		GuidToParameterName.Reserve(MaterialDefinition->Parameters.Num());
		for (const FVoxelParameter& Parameter : MaterialDefinition->Parameters)
		{
			GuidToParameterName.Add_CheckNew(Parameter.Guid, Parameter.Name);
		}

		for (const auto& It : MaterialDefinition->GuidToParameterData)
		{
			const FName* NamePtr = GuidToParameterName.Find(It.Key);
			if (!ensure(NamePtr))
			{
				continue;
			}

			It.Value->CacheParameters(
				FName("VOXELPARAM_" + NamePtr->ToString()),
				*CachedParameters);
		}
	}
}

void FVoxelMaterialDefinitionManager::SetAllParameters(UMaterialInstanceDynamic& Instance)
{
	VOXEL_FUNCTION_COUNTER();

	if (!CachedParameters)
	{
		CacheParameters();
	}

	for (const auto& It : CachedParameters->ScalarParameters)
	{
		VOXEL_SCOPE_COUNTER("SetScalarParameterValue");
		Instance.SetScalarParameterValue(It.Key, It.Value);
	}
	for (const auto& It : CachedParameters->VectorParameters)
	{
		VOXEL_SCOPE_COUNTER("SetVectorParameterValue");
		Instance.SetVectorParameterValue(It.Key, It.Value);
	}
	for (const auto& It : CachedParameters->TextureParameters)
	{
		VOXEL_SCOPE_COUNTER("SetTextureParameterValue");
		Instance.SetTextureParameterValue(It.Key, It.Value.Get());
	}
}