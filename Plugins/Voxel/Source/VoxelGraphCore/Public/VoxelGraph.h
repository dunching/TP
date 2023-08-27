// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelParameter.h"
#include "VoxelCompiledGraph.h"
#include "VoxelGraphInterface.h"
#include "VoxelGraph.generated.h"

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelEditedDocumentInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FSoftObjectPath EditedObjectPath;

	UPROPERTY()
	FVector2D SavedViewOffset = FVector2D::ZeroVector;

	UPROPERTY()
	float SavedZoomAmount = -1.f;

	FVoxelEditedDocumentInfo() = default;

	explicit FVoxelEditedDocumentInfo(const UObject* EditedObject)
		: EditedObjectPath(EditedObject)
	{
	}

	FVoxelEditedDocumentInfo(
		const UObject* EditedObject,
		const FVector2D& SavedViewOffset,
		const float SavedZoomAmount)
		: EditedObjectPath(EditedObject)
		, SavedViewOffset(SavedViewOffset)
		, SavedZoomAmount(SavedZoomAmount)
	{
	}
};

UENUM()
enum class EVoxelGraphParameterType
{
	Parameter,
	Input,
	Output,
	LocalVariable
};
ENUM_RANGE_BY_FIRST_AND_LAST(
	EVoxelGraphParameterType,
	EVoxelGraphParameterType::Parameter,
	EVoxelGraphParameterType::LocalVariable);

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelGraphParameter : public FVoxelParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Voxel", DisplayName = "Expose Default as Pin")
	bool bExposeInputDefaultAsPin = false;

	UPROPERTY(EditAnywhere, Category = "Voxel")
	EVoxelGraphParameterType ParameterType = {};
};

USTRUCT()
struct VOXELGRAPHCORE_API FVoxelGraphPreviewConfig
{
	GENERATED_BODY()

	UPROPERTY()
	EVoxelAxis Axis = EVoxelAxis::X;

	UPROPERTY()
	int32 Resolution = 512;

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	double Zoom = 1.;

	float GetAxisLocation() const
	{
		switch (Axis)
		{
		default: return 0.f;
		case EVoxelAxis::X: return Position.X;
		case EVoxelAxis::Y: return Position.Y;
		case EVoxelAxis::Z: return Position.Z;
		}
	}
	void SetAxisLocation(const float Value)
	{
		switch (Axis)
		{
		default: ensure(false);
		case EVoxelAxis::X: Position.X = Value; break;
		case EVoxelAxis::Y: Position.Y = Value; break;
		case EVoxelAxis::Z: Position.Z = Value; break;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS()
class VOXELGRAPHCORE_API UVoxelGraph : public UVoxelGraphInterface
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap<FName, TObjectPtr<UEdGraph>> Graphs_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UEdGraph> MainEdGraph;

	UPROPERTY()
	TArray<FVoxelEditedDocumentInfo> LastEditedDocuments;
#endif

	UPROPERTY()
	TObjectPtr<UVoxelGraph> Graph_DEPRECATED;

	UPROPERTY()
	TArray<TObjectPtr<UVoxelGraph>> InlineMacros;

	UPROPERTY()
	FVoxelParameterCategories InlineMacroCategories;

public:
#if WITH_EDITORONLY_DATA
	// Choose title bar color for this macro usages
	UPROPERTY(EditAnywhere, Category = "Config")
	FLinearColor InstanceColor = FLinearColor::Gray;
#endif

	UPROPERTY(EditAnywhere, Category = "Config")
	FString Tooltip;

	UPROPERTY(EditAnywhere, Category = "Config", AssetRegistrySearchable)
	FString Category;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Config", AssetRegistrySearchable)
	FString Description;
#endif

	UPROPERTY(EditAnywhere, Category = "Internal")
	TArray<FVoxelGraphParameter> Parameters;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Internal")
	TMap<FGuid, TObjectPtr<UVoxelGraph>> ParameterGraphs;

	UPROPERTY()
	bool bParameterGraph = false;
#endif

	UPROPERTY()
	TMap<EVoxelGraphParameterType, FVoxelParameterCategories> ParametersCategories;

	UPROPERTY()
	FVoxelCompiledGraph CompiledGraph;

	UPROPERTY()
	FVoxelGraphPreviewConfig Preview;

	UPROPERTY()
	bool bExposeToLibrary = false;

	FSimpleMulticastDelegate OnGraphCompiled;

	enum class EParameterChangeType
	{
		Unknown,
		DefaultValue
	};

	DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelGraphParameterChange, EParameterChangeType);
	mutable FVoxelGraphParameterChange OnParametersChanged;

public:
	UVoxelGraph();

	virtual FString GetGraphName() const override;
	void SetGraphName(const FString& NewName);

private:
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bEnableNameOverride = false;

	UPROPERTY(EditAnywhere, DisplayName = "Name", Category = "Config")
	FString NameOverride;

	friend class FUVoxelGraphCustomization;

public:
	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostCDOContruct() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	//~ Begin UVoxelGraphInterface Interface
	virtual UVoxelGraph* GetGraph() const override
	{
		return ConstCast(this);
	}
	//~ End UVoxelGraphInterface Interface

	//~ Begin IVoxelParameterProvider Interface
	virtual void AddOnChanged(const FSimpleDelegate& Delegate) override;
	virtual TSharedPtr<IVoxelParameterRootView> GetParameterViewImpl(const FVoxelParameterPath& BasePath) override;
	//~ End IVoxelParameterProvider Interface

	void PostGraphCompiled() const;

	void FixupParameters();
	TArray<UVoxelGraph*> GetAllGraphs();
	TArray<const UVoxelGraph*> GetAllGraphs() const
	{
		return ReinterpretCastArray<const UVoxelGraph*>(ConstCast(this)->GetAllGraphs());
	}

#if WITH_EDITOR
	TArray<UVoxelGraph*> GetAllParameterGraphs();
	UVoxelGraph* FindGraph(const UEdGraph* EdGraph);
	const UVoxelGraph* FindGraph(const UEdGraph* EdGraph) const
	{
		return ConstCast(this)->FindGraph(EdGraph);
	}
#endif

	FVoxelGraphParameter* FindParameterByGuid(const FGuid& TargetGuid)
	{
		return Parameters.FindByKey(TargetGuid);
	}
	const FVoxelGraphParameter* FindParameterByGuid(const FGuid& TargetGuid) const
	{
		return Parameters.FindByKey(TargetGuid);
	}

	FVoxelGraphParameter* FindParameterByName(const EVoxelGraphParameterType ParameterType, const FName TargetName)
	{
		return Parameters.FindByPredicate([&](const FVoxelGraphParameter& Parameter)
		{
			return
				Parameter.ParameterType == ParameterType &&
				Parameter.Name == TargetName;
		});
	}

	TArray<FString>& GetCategories(const EVoxelGraphParameterType Type)
	{
		return ParametersCategories[Type].Categories;
	}
	const TArray<FString>& GetCategories(const EVoxelGraphParameterType Type) const
	{
		return ParametersCategories[Type].Categories;
	}

private:
	void FixupCategories();
	void FixupInlineMacroCategories();
};