// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IFC : ModuleRules
{
	public IFC(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"FlecsLibrary",
			"ECSCore",
			"UIElements",
		});


		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"Engine",
			"Json",
		});
	}
}
