// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include <flecs.h>

class FIFCModule : public IModuleInterface {
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

namespace IFC {
	constexpr const char* DATA = "data";
	constexpr const char* PATH = "path";
	constexpr const char* ATTRIBUTES = "attributes";
	constexpr const char* INHERITS = "inherits";
	constexpr const char* CHILDREN = "children";

	constexpr const char* DIFFUSECOLOR_COMPONENT = "bsi_ifc_presentation_diffuseColor";
	constexpr const char* OPACITY_ATTRIBUTE = "bsi::ifc::presentation::opacity";

	static const TSet<FString> ExcludeAttributes = {
		TEXT("customdata"),
		TEXT("bsi::ifc::presentation::opacity"),
	};

	static const TSet<FString> ExcludeAtributesValues = {
		TEXT("bsi::ifc::spaceBoundary"),
		TEXT("usd::usdgeom::mesh"),
		TEXT("usd::xformop"),
		TEXT("usd::usdgeom::basiscurves"),
	};

	static const TSet<FString> VectorAttributes = {
		TEXT("bsi::ifc::presentation::diffuseColor"),
	};

	static const TSet<FString> EnumAttributes = {
		TEXT("bsi::ifc::prop::FireRating"),
	};

	IFC_API FString& Scope();

	IFC_API void Register(flecs::world& world);

	IFC_API void LoadIFCFiles(flecs::world& world, const TArray<FString>& paths);
}
