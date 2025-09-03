// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ECS.h"
#include <flecs.h>
#include "rapidjson/document.h"

namespace IFC {
	struct AttributeFeature {
		static void RegisterComponents(flecs::world& world);
	};

    constexpr const char* ATTRIBUTES = "attributes";

	inline constexpr TCHAR TOKEN_VALUE[] = TEXT("[VALUE]");

	struct Attribute {};
	struct Value { FString Value; };

	using namespace rapidjson; 

	FString GetAttributes(const rapidjson::Value& object);
}
