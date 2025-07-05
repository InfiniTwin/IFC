// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <flecs.h>

namespace IFC {
	struct IFCFeature {
		static void RegisterComponents(flecs::world& world);
	};

	struct Id { FString Value; };
	struct Name { FString Value; };
	struct Class { FString Value; };
}