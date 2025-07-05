// Fill out your copyright notice in the Description page of Project Settings.

#include "IFCFeature.h"
#include <ECS.h>

namespace IFC {
	void IFCFeature::RegisterComponents(flecs::world& world) {
		using namespace ECS;
		world.component<Id>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Name>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Class>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
	}
}