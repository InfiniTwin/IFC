// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include <flecs.h>
#include "rapidjson/document.h"

class FIFCModule : public IModuleInterface {
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

namespace IFC {
	IFC_API FString& Scope();

	IFC_API void Register(flecs::world& world);

	// List of allowed attributes / schemas
	constexpr const TCHAR* AllowedAttributes[] = {
		// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/ifc@v5a.ifcx
		TEXT("diffuseColor"),
		TEXT("opacity"),
		TEXT("class"),
		//TEXT("spaceBoundary"),
		TEXT("material"),
		//TEXT("alignment"),
		//TEXT("alignmenthorizontalsegment"),
		//TEXT("alignmentverticalsegment"),
		//TEXT("alignmentcantsegment"), 
		//TEXT("flowdirection"),
		//TEXT("partofsystem"),

		// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/prop@v5a.ifcx
		TEXT("IsExternal"),
		TEXT("Volume"),
		TEXT("Height"),
		TEXT("Station"),
		TEXT("TypeName"),

		// https://ifcx.dev/@standards.buildingsmart.org/ifc/ifc-mat/prop@v1.0.0.ifcx
		TEXT("StrengthClass"),
		TEXT("MoistureContent"),
		TEXT("MassDensity"),
		//TEXT("GWP"),

		// https://ifcx.dev/@openusd.org/usd@v1.ifcx
		//TEXT("mesh"),
		//TEXT("visibility"),
		//TEXT("xformop"),
		//TEXT("basiscurves"),
	};

	FString FormatUUIDs(const FString& input);

	bool IncludeAttribute(const FString& attrName);
	FString ProcessAttributes(const rapidjson::Value& attributes, const TArray<FString>& filteredAttrNames);
	FString GetPrefabs(const rapidjson::Value& data);

	FString BuildHierarchyTree(const rapidjson::Value* root, const TMap<FString, const rapidjson::Value*>& pathToObjectMap, int32 depth = 0);
	FString GetHierarchies(const rapidjson::Value& data);

	void LoadIFCFile(flecs::world& world, const FString& path);
}
