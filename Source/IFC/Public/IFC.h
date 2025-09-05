// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "ECS.h"
#include <flecs.h>
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

class FIFCModule : public IModuleInterface {
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

namespace IFC {
#pragma region IFC
	constexpr const char* DATA = "data";
	constexpr const char* PATH = "path";
	constexpr const char* INHERITS = "inherits";
	constexpr const char* CHILDREN = "children";
	constexpr const TCHAR* PREFAB = TEXT("prefab ");

	inline constexpr TCHAR TOKEN_NAME[] = TEXT("[NAME]");

	using namespace rapidjson;

	FString FormatUUID(const FString& input);
	FString FormatName(const FString& fullName);

	IFC_API void LoadIfcData(flecs::world& world, const TArray<flecs::entity> layers);
#pragma endregion

#pragma region Flecs
	IFC_API FString& Scope();

	IFC_API void Register(flecs::world& world);

	struct Name { FString Value; };
	struct IfcObject {};
	struct QueryIfcData { flecs::query<> Value; };

	struct Root {};
	struct Branch {};
#pragma endregion
}
