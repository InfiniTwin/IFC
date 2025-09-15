// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <flecs.h>

namespace IFC {
	struct ModelFeature {
		static void CreateComponents(flecs::world& world);
		static void CreateObservers(flecs::world& world);
		static void Initialize(flecs::world& world);
	};

	constexpr const float TO_CM = 100;

	struct Position { FVector Value; };
	struct Rotation { FRotator Value; };
	struct Scale { FVector Value; };

	struct Mesh { int32 Value; };
	struct ISM { uint64 Value; };
	struct Material { int32 Value; };

	FTransform ToTransform(const float values[4][4]);
	int32 CreateMesh(flecs::world& world, TArray<FVector3f> points, TArray<int32> indices);
	int32 CreateMaterial(flecs::world& world, const FVector4f& rgba, float offset);
}
