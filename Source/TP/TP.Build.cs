// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TP : ModuleRules
{
	public TP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[] {
            "VoxelCore",
            "VoxelGraphCore",
            "EnhancedInput" ,
        });

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput" });
	}
}
