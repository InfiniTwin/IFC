// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <flecs.h>
#include "rapidjson/document.h"

namespace IFC {
	struct AttributeFeature {
		static void CreateComponents(flecs::world& world);
		static void Initialize(flecs::world& world);
	};

	inline constexpr TCHAR TOKEN_VALUE[] = TEXT("[VALUE]");

	constexpr const char* ATTRIBUTES_KEY = "attributes";
	// Classes
	constexpr const char* ATTRIBUTE_IFC_CLASS = "bsi::ifc::class";
	constexpr const char* IFC_CLASS_CODE = "code";
	constexpr const char* IFC_SPACE = "IfcSpace";
	// Transform
	constexpr const char* ATTRIBUTE_XFORMOP = "usd::xformop";
	constexpr const char* ATTRIBUTE_TRANSFROM = "transform";
	// Mesh
	constexpr const char* ATTRIBUTE_MESH = "usd::usdgeom::mesh";
	constexpr const char* MESH_INDICES = "faceVertexIndices";
	constexpr const char* MESH_POINTS = "points";
	// Material
	constexpr const char* ATTRIBUTE_MATERIAL = "bsi::ifc::material";
	constexpr const char* ATTRIBUTE_DIFFUSECOLOR = "bsi::ifc::presentation::diffuseColor";
	constexpr const char* ATTRIBUTE_OPACITY = "bsi::ifc::presentation::opacity";
	constexpr const char* ATTRIBUTE_VISIBILITY = "usd::usdgeom::visibility";
	constexpr const char* VISIBILITY_VISIBILITY = "visibility";
	constexpr const char* VISIBILITY_INVISIBLE = "invisible";
	// Relationships
	constexpr const char* ATTRIBUTE_SPACE_BOUNDARY = "bsi::ifc::spaceBoundary";
	constexpr const char* RELATED_ELEMENT = "relatedelement";
	constexpr const char* RELATING_SPACE = "relatingspace";

	constexpr const char* ATTRIBUTE_ALIGNMENT = "bsi::ifc::alignment";


	static const TSet<FString> ExcludeAttributes = {
		ATTRIBUTE_OPACITY,
		ATTRIBUTE_ALIGNMENT
	};

	struct AttributeRelationship { flecs::entity Value; };

	struct Attribute {};
	struct Value { FString Value; };

	// Entities
	struct Alignment {};
	struct AlignmentCant {};
	struct AlignmentHorizontal {};
	struct AlignmentSegment {};
	struct AlignmentVertical {};
	struct Building {};
	struct BuildingStorey {};
	struct Project {};
	struct Railway {};
	struct Referent {};
	struct Signal {};
	struct Site {};
	struct Space {};
	struct Wall {};
	struct Window {};

	// Relationships
	struct SpaceBoundary {};
	struct RelatedElement { flecs::entity Value; };
	struct RelatingSpace { flecs::entity Value; };

	constexpr const char* ATTRIBUTE_SEPARATOR = "::";

	TTuple<FString, FString, FString> GetAttributes(flecs::world& world, const rapidjson::Value& object, const FString& objectPath);
}
