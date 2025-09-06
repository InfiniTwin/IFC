// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ECS.h"
#include <flecs.h>
#include "rapidjson/document.h"

namespace IFC {
	struct ModelFeature {
		static void CreateComponents(flecs::world& world);
		static void CreateObservers(flecs::world& world);
	};

	constexpr const char* ATTRIBUTE_MESH = "usd::usdgeom::mesh";
	constexpr const char* MESH_INDICES = "faceVertexIndices";
	constexpr const char* MESH_POINTS = "points";

	constexpr const char* ATTRIBUTE_XFORMOP = "usd::xformop";
	constexpr const char* ATTRIBUTE_TRANSFROM = "transform";

	struct Position { FVector Value; };
	struct Rotation { FRotator Value; };
	struct Scale { FVector Value; };

	struct Mesh { int32 Value; };
	struct ISM { uint64 Value; };

	FTransform ToTransform(const rapidjson::Value& xformop);

	int32 CreateMesh(flecs::world& world, const rapidjson::Value& value);
}
