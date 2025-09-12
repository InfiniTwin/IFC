// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ECS.h"
#include <flecs.h>
#include "rapidjson/document.h"

namespace IFC {
	struct AttributeFeature {
		static void CreateComponents(flecs::world& world);
		static void Initialize(flecs::world& world);
	};

	inline constexpr TCHAR TOKEN_VALUE[] = TEXT("[VALUE]");

	constexpr const char* ATTRIBUTES_KEY = "attributes";

	constexpr const char* ATTRIBUTE_XFORMOP = "usd::xformop";
	constexpr const char* ATTRIBUTE_TRANSFROM = "transform";

	constexpr const char* ATTRIBUTE_MESH = "usd::usdgeom::mesh";
	constexpr const char* MESH_INDICES = "faceVertexIndices";
	constexpr const char* MESH_POINTS = "points";

	constexpr const char* ATTRIBUTE_MATERIAL = "bsi::ifc::material";
	constexpr const char* ATTRIBUTE_DIFFUSECOLOR = "bsi::ifc::presentation::diffuseColor";
	constexpr const char* ATTRIBUTE_OPACITY = "bsi::ifc::presentation::opacity";

	constexpr const char* ATTRIBUTE_VISIBILITY = "usd::usdgeom::visibility";
	constexpr const char* VISIBILITY_VISIBILITY = "visibility";
	constexpr const char* VISIBILITY_INVISIBLE = "invisible";

	static const TSet<FString> ExcludeAttributes = {
		ATTRIBUTE_OPACITY,
	};

	struct Attribute {};
	struct Value { FString Value; };
	struct AttributeRelationship { flecs::entity Value; };

	constexpr const char* ATTRIBUTE_SEPARATOR = "::";

	TTuple<FString, FString> GetAttributes(flecs::world& world, const rapidjson::Value& object, const FString& objectPath);
}
