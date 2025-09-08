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

	constexpr const char* ATTRIBUTE_XFORMOP = "usd::xformop";
	constexpr const char* ATTRIBUTE_TRANSFROM = "transform";

	constexpr const char* ATTRIBUTE_MESH = "usd::usdgeom::mesh";
	constexpr const char* MESH_INDICES = "faceVertexIndices";
	constexpr const char* MESH_POINTS = "points";

	constexpr const char* ATTRIBUTE_DIFFUSECOLOR = "bsi::ifc::presentation::diffuseColor";
	constexpr const char* ATTRIBUTE_OPACITY = "bsi::ifc::presentation::opacity";
	
	constexpr const char* ATTRIBUTE_VISIBILITY = "usd::usdgeom::visibility";
	constexpr const char* VISIBILITY_INVISIBLE = "invisible";

	constexpr const float TO_CM = 100;

	struct Position { FVector Value; };
	struct Rotation { FRotator Value; };
	struct Scale { FVector Value; };

	struct Mesh { int32 Value; };
	struct ISM { uint64 Value; };
	struct Material { int32 Value; };

	FTransform ToTransform(const float values[4][4]);
	int32 CreateMesh(flecs::world& world, TArray<FVector3f> points, TArray<int32> indices);
	int32 CreateMaterial(flecs::world& world, const FVector4f& rgba);
}
