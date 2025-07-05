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

	FString FormatUUIDs(const FString& input);

	FString BuildHierarchyTree(const rapidjson::Value* root, const TMap<FString, const rapidjson::Value*>& pathToObjectMap, int32 depth = 0);

	FString GetHierarchies(const FString& jsonString);

	void LoadIFCFile(flecs::world& world, const FString& path);
}
