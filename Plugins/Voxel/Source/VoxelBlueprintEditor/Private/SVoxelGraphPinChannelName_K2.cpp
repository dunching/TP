// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SVoxelGraphPinChannelName_K2.h"
#include "VoxelChannel.h"

void SVoxelGraphPinChannelName_K2::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SVoxelGraphPinChannelName_K2::GetDefaultValueWidget()
{
	return
		SNew(SVoxelDetailComboBox<FName>)
		.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
		.NoRefreshDelegate()
		.Options_Lambda([]
		{
			return GVoxelChannelManager->GetValidChannelNames();
		})
		.CurrentOption(FName(GraphPinObj->DefaultValue))
		.CanEnterCustomOption(true)
		.OptionText(MakeLambdaDelegate([](const FName Option)
		{
			return Option.ToString();
		}))
		.OnSelection_Lambda([this](const FName NewValue)
		{
			if (!ensure(!GraphPinObj->IsPendingKill()))
			{
				return;
			}

			const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
			if (!ensure(Schema))
			{
				return;
			}

			if (GraphPinObj->GetDefaultAsString() == NewValue.ToString())
			{
				return;
			}

			const FScopedTransaction Transaction(INVTEXT("Set channel name"));
			GraphPinObj->Modify();
			Schema->TrySetDefaultValue(*GraphPinObj, NewValue.ToString());
		});
}