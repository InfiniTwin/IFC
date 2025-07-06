// Fill out your copyright notice in the Description page of Project Settings.

#include "IFCFeature.h"
#include <ECS.h>

namespace IFC {
	void IFCFeature::RegisterComponents(flecs::world& world) {
		using namespace ECS;

		// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/ifc@v5a.ifcx
		//world.component<TArray<float>>().opaque(ArrayReflection<float>);
		world.component<DiffuseColor>().member<TArray<float>>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Opacity>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Class>()
			.member<FString>(MEMBER(Class::Code))
			.member<FString>(MEMBER(Class::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Material>()
			.member<FString>(MEMBER(Material::Code))
			.member<FString>(MEMBER(Material::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);

		// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/prop@v5a.ifcx
		world.component<IsExternal>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Volume>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Height>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Station>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<TypeName>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);

		// https://ifcx.dev/@standards.buildingsmart.org/ifc/ifc-mat/prop@v1.0.0.ifcx
		world.component<StrengthClass>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<MoistureContent>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<MassDensity>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
	}
}