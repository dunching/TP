// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelGraph.h"
#include "Widgets/SVoxelMembers.h"

BEGIN_VOXEL_NAMESPACE(Graph)

enum class EMembersNodeSection
{
	None,
	Graph,
	InlineMacros,
	MacroLibraries,
	Parameters,
	MacroInputs,
	MacroOutputs,
	LocalVariables,
};

FORCEINLINE int32 GetSectionId(const EMembersNodeSection Type)
{
	switch (Type)
	{
	default: check(false);
	case EMembersNodeSection::None: return 0;
	case EMembersNodeSection::Graph: return 1;
	case EMembersNodeSection::InlineMacros: return 2;
	case EMembersNodeSection::MacroLibraries: return 3;
	case EMembersNodeSection::Parameters: return 4;
	case EMembersNodeSection::MacroInputs: return 5;
	case EMembersNodeSection::MacroOutputs: return 6;
	case EMembersNodeSection::LocalVariables: return 7;
	}
}
FORCEINLINE EMembersNodeSection GetSection(const int32 SectionId)
{
	switch (SectionId)
	{
	case 1: return EMembersNodeSection::Graph;
	case 2: return EMembersNodeSection::InlineMacros;
	case 3: return EMembersNodeSection::MacroLibraries;
	case 4: return EMembersNodeSection::Parameters;
	case 5: return EMembersNodeSection::MacroInputs;
	case 6: return EMembersNodeSection::MacroOutputs;
	case 7: return EMembersNodeSection::LocalVariables;
	default: ensure(SectionId == 0); return EMembersNodeSection::None;
	}
}

FORCEINLINE EMembersNodeSection GetSection(const EVoxelGraphParameterType Type)
{
	switch (Type)
	{
	default: ensure(false);
	case EVoxelGraphParameterType::Parameter: return EMembersNodeSection::Parameters;
	case EVoxelGraphParameterType::Input: return EMembersNodeSection::MacroInputs;
	case EVoxelGraphParameterType::Output: return EMembersNodeSection::MacroOutputs;
	case EVoxelGraphParameterType::LocalVariable: return EMembersNodeSection::LocalVariables;
	}
}
FORCEINLINE int32 GetSectionId(const EVoxelGraphParameterType Type)
{
	return GetSectionId(GetSection(Type));
}

FORCEINLINE EVoxelGraphParameterType GetParameterType(const EMembersNodeSection Type)
{
	switch (Type)
	{
	default: ensure(false);
	case EMembersNodeSection::Parameters: return EVoxelGraphParameterType::Parameter;
	case EMembersNodeSection::MacroInputs: return EVoxelGraphParameterType::Input;
	case EMembersNodeSection::MacroOutputs: return EVoxelGraphParameterType::Output;
	case EMembersNodeSection::LocalVariables: return EVoxelGraphParameterType::LocalVariable;
	}
}

class SMembers : public SVoxelMembers
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ARGUMENT(UVoxelGraph*, Graph);
		SLATE_ARGUMENT(TSharedPtr<FVoxelToolkit>, Toolkit);
	};

	void Construct(const FArguments& Args);
	void UpdateActiveGraph(const TWeakObjectPtr<UVoxelGraph>& NewActiveGraph);

protected:
	//~ Begin SVoxelMembers Interface
	virtual void CollectStaticSections(TArray<int32>& StaticSectionIds) override;
	virtual FText OnGetSectionTitle(int32 SectionId) override;
	virtual TSharedRef<SWidget> OnGetMenuSectionWidget(TSharedRef<SWidget> RowWidget, int32 SectionId) override;
	virtual void CollectSortedActions(FVoxelMembersActionsSortHelper& OutActionsList) override;
	virtual void SelectBaseObject() override;
	virtual void GetContextMenuAddOptions(FMenuBuilder& MenuBuilder) override;
	virtual void OnPasteItem(const FString& ImportText, int32 SectionId) override;
	virtual bool CanPasteItem(const FString& ImportText, int32 SectionId) override;
	virtual void OnAddNewMember(int32 SectionId) override;
	virtual const TArray<FString>& GetCopyPrefixes() const override;

public:
	virtual TArray<FString>& GetEditableCategories(int32 SectionId) override;
	//~ End SVoxelMembers Interface

private:
	void OnParametersChanged(UVoxelGraph::EParameterChangeType ChangeType);

private:
	FDelegateHandle OnMembersChangedHandle;
};

END_VOXEL_NAMESPACE(Graph)