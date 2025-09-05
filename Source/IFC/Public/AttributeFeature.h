// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ECS.h"
#include <flecs.h>
#include "rapidjson/document.h"

namespace IFC {
	struct AttributeFeature {
		static void CreateComponents(flecs::world& world);
	};

    constexpr const char* ATTRIBUTES = "attributes";

	inline constexpr TCHAR TOKEN_VALUE[] = TEXT("[VALUE]");

	struct Attribute {};
	struct Value { FString Value; };
	struct Mesh { int32 Value; };

	constexpr const char* ATTRIBUTE_SEPARATOR = "::";

	constexpr const char* ATTRIBUTE_MESH = "usd::usdgeom::mesh";
	constexpr const char* MESH_INDICES = "faceVertexIndices";
	constexpr const char* MESH_POINTS = "points";

	using namespace rapidjson; 

	TTuple<FString, FString> GetAttributes(const rapidjson::Value& object, const FString& objectPath);
}
