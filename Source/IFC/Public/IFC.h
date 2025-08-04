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
	constexpr const char* ATTRIBUTES = "attributes";
	constexpr const char* ATTRIBUTE_SEPARATOR = "::";
	constexpr const char* INHERITS = "inherits";
	constexpr const char* CHILDREN = "children";
	constexpr const TCHAR* PREFAB = TEXT("prefab ");

	constexpr const char* DIFFUSECOLOR_COMPONENT = "bsi_ifc_presentation_diffuseColor";
	constexpr const char* OPACITY_ATTRIBUTE = "bsi::ifc::presentation::opacity";

	static const TSet<FString> ExcludeAttributes = {
		TEXT("customdata"),
		TEXT("bsi::ifc::presentation::opacity"),
	};

	static const TSet<FString> ExcludeAtributesValues = {
		TEXT("usd::usdgeom::mesh"),
		TEXT("usd::xformop"),
		TEXT("usd::usdgeom::basiscurves"),
	};

	static const TSet<FString> EnumAttributes = {
		TEXT("bsi::ifc::prop::FireRating"),
	};

	static const TSet<FString> VectorAttributes = {
		TEXT("bsi::ifc::presentation::diffuseColor"),
	};

	static const TSet<FString> RelationshipAttributes = {
		TEXT("bsi::ifc::spaceBoundary"),
	};

	using namespace rapidjson;

	FString FormatUUID(const FString& input);
	FString FormatName(const FString& fullName);
	FString FormatAttributeValue(const Value& value, bool isInnerArray = false);

	IFC_API void LoadIFCData(flecs::world& world, const TArray<flecs::entity> layers);
#pragma endregion

#pragma region Flecs
	IFC_API FString& Scope();

	IFC_API void Register(flecs::world& world);

	// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/ifc@v5a.ifcx
	struct bsi_ifc_presentation_diffuseColor { FLinearColor Value; };
	struct bsi_ifc_class { FString Code, Uri; };
	struct bsi_ifc_spaceBoundary_relatedelement { FString Value; };
	struct bsi_ifc_spaceBoundary_relatingspace { FString Value; };
	struct bsi_ifc_material { FString Code, Uri; };

	// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/prop@v5a.ifcx
	struct bsi_ifc_prop_IsExternal { bool Value; };
	struct bsi_ifc_prop_Volume { float Value; };
	struct bsi_ifc_prop_Height { float Value; };
	struct bsi_ifc_prop_Station { float Value; };
	struct bsi_ifc_prop_TypeName { FString Value; };
	enum bsi_ifc_prop_FireRating { R30, R60 };

	// https://ifcx.dev/@standards.buildingsmart.org/ifc/ifc-mat/prop@v1.0.0.ifcx
	struct bsi_ifc_mat_prop_StrengthClass { FString Value; };
	struct bsi_ifc_mat_prop_MoistureContent { float Value; };
	struct bsi_ifc_mat_prop_MassDensity { float Value; };
	struct bsi_ifc_mat_prop_GWP { float A1_A3, A4, A5, C2, C3, D; };

	// https://ifcx.dev/@openusd.org/usd@v1.ifcx
	struct usd_usdgeom_mesh {
		TArray<int> FaceVertexIndices;
		TArray<FVector3f> Points;
	};
	struct usd_usdgeom_visibility { FString Value; };
	struct usd_xformop { TArray<FVector4f> Transform; };
	struct usd_usdgeom_basiscurves { TArray<FVector3f> Points; };

	// "https://ifcx.dev/@nlsfb/nlsfb@v1.ifcx"
	struct nlsfb_class { FString Code, Uri; };
#pragma endregion
}
