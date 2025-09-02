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

	struct Attribute { FString Value; };

	using namespace rapidjson; 

	FString GetAttributes(const rapidjson::Value& object);
}
