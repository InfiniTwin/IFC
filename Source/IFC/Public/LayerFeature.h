// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ECS.h"
#include <flecs.h>

namespace IFC {
	struct LayerFeature{
		static void CreateComponents(flecs::world& world);
		static void CreateQueries(flecs::world& world);
	};

	constexpr const char* HEADER = "header";
	constexpr const char* OWNER = COMPONENT(Owner);

	struct Layer {};
	struct Path { FString Value; };
	struct IfcxVersion { FString Value; };
	struct DataVersion { FString Value; };
	struct Author { FString Value; };
	struct Timestamp { FString Value; };

	struct Owner { FString Value; };

	struct QueryLayers { flecs::query<> Value; };

	IFC_API FString CleanLayerName(const FString& in);
	IFC_API void AddLayers(flecs::world& world, const TArray<FString>& paths, const TArray<FString>& components);

	FString GetOwnerPath(const FString& layerPath);
}
