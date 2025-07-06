// Fill out your copyright notice in the Description page of Project Settings.

#include "IFCFeature.h"
#include "ECS.h"

namespace IFC {
	void IFCFeature::RegisterComponents(flecs::world& world) {
		using namespace ECS;

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

		world.component<IsExternal>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Volume>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Height>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Station>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<TypeName>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);

		world.component<StrengthClass>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<MoistureContent>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<MassDensity>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<GWP>()
			.member<float>(MEMBER(GWP::A1_A3))
			.member<float>(MEMBER(GWP::A4))
			.member<float>(MEMBER(GWP::A5))
			.member<float>(MEMBER(GWP::C2))
			.member<float>(MEMBER(GWP::C3))
			.member<float>(MEMBER(GWP::D))
			.add(flecs::OnInstantiate, flecs::Inherit);

		world.component<Mesh>()
			.member<TArray<float>>(MEMBER(Mesh::FaceVertexIndices))
			.member<TArray<FVector3f>>(MEMBER(Mesh::Points))
			.add(flecs::OnInstantiate, flecs::Inherit);
	}
}