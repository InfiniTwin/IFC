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
		});


		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"Engine",
		});
	}
}
