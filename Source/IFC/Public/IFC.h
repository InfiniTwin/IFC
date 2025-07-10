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
	static const TSet<FString> AllowedAttributes = {
		// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/ifc@v5a.ifcx
		TEXT("bsi::ifc::presentation::opacity"),
		TEXT("bsi::ifc::class"),
		//TEXT("bsi::ifc::spaceBoundary"),
		TEXT("bsi::ifc::material"),
		//TEXT("bsi::ifc::alignment"),
		//TEXT("bsi::ifc::alignmenthorizontalsegment"),
		//TEXT("bsi::ifc::alignmentverticalsegment"),
		//TEXT("bsi::ifc::alignmentcantsegment"), 
		//TEXT("bsi::ifc::system::connectsto"), 
		//TEXT("bsi::ifc::system::flowdirection"),
		//TEXT("bsi::ifc::system::partofsystem"),

		// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/prop@v5a.ifcx
		TEXT("bsi::ifc::prop::IsExternal"),
		TEXT("bsi::ifc::prop::Volume"),
		TEXT("bsi::ifc::prop::Height"),
		TEXT("bsi::ifc::prop::Station"),
		TEXT("bsi::ifc::prop::TypeName"),

		// https://ifcx.dev/@standards.buildingsmart.org/ifc/ifc-mat/prop@v1.0.0.ifcx
		TEXT("bsi::ifc-mat::prop::StrengthClass"),
		TEXT("bsi::ifc-mat::prop::MoistureContent"),
		TEXT("bsi::ifc-mat::prop::MassDensity"),
		TEXT("bsi::ifc-mat::prop::GWP"),

		// https://ifcx.dev/@openusd.org/usd@v1.ifcx
		//TEXT("usd::usdgeom::mesh"),
		TEXT("usd::usdgeom::visibility"),
		//TEXT("usd::xformop"),
		//TEXT("usd::usdgeom::basiscurves"),

		// "https://ifcx.dev/@nlsfb/nlsfb@v1.ifcx"
		TEXT("nlsfb::class"),
	};

	static const TSet<FString> VectorAttributes = {
		// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/ifc@v5a.ifcx
		TEXT("bsi::ifc::presentation::diffuseColor"),
	};
	static const TSet<FString> SkipProcessingAttributes = {
		// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/ifc@v5a.ifcx
		TEXT("bsi::ifc::presentation::opacity"),
	};

	constexpr const char* DIFFUSECOLOR_COMPONENT = "bsi_ifc_presentation_diffuseColor";
	constexpr const char* OPACITY_ATTRIBUTE = "bsi::ifc::presentation::opacity";

	IFC_API FString& Scope();

	IFC_API void Register(flecs::world& world);

	IFC_API void LoadIFCFile(flecs::world& world, const FString& path);
}
