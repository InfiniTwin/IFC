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

	constexpr const char* ATTRIBUTE_SEPARATOR = "::";

	TTuple<FString, FString> GetAttributes(flecs::world& world, const rapidjson::Value& object, const FString& objectPath);
}
