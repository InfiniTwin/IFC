// Fill out your copyright notice in the Description page of Project Settings.

#include "IFCFeature.h"
#include "IFC.h"
#include "ActionFeature.h"
#include "Assets.h"

namespace IFC {
	void IFCFeature::RegisterComponents(flecs::world& world) {
		using namespace ECS;

		world.component<Layer>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);

#pragma region Attributes
		world.component<bsi_ifc_presentation_diffuseColor>().member<FLinearColor>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_class>()
			.member<FString>(MEMBER(bsi_ifc_class::Code))
			.member<FString>(MEMBER(bsi_ifc_class::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_spaceBoundary>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_material>()
			.member<FString>(MEMBER(bsi_ifc_material::Code))
			.member<FString>(MEMBER(bsi_ifc_material::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);

		world.component<bsi_ifc_prop_IsExternal>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_Volume>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_Height>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_Station>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_TypeName>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_FireRating>().add(flecs::Exclusive).add(flecs::OnInstantiate, flecs::Inherit);

		world.component<bsi_ifc_mat_prop_StrengthClass>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_mat_prop_MoistureContent>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_mat_prop_MassDensity>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_mat_prop_GWP>()
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::A1_A3))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::A4))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::A5))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::C2))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::C3))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::D))
			.add(flecs::OnInstantiate, flecs::Inherit);

		world.component<usd_usdgeom_mesh>()
			.member<TArray<float>>(MEMBER(usd_usdgeom_mesh::FaceVertexIndices))
			.member<TArray<FVector3f>>(MEMBER(usd_usdgeom_mesh::Points))
			.add(flecs::OnInstantiate, flecs::Inherit);
		world.component<usd_usdgeom_visibility>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);

		using namespace ECS;
		world.component<usd_xformop>().member<TArray<FVector4f>>(MEMBER(usd_xformop::Transform)).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<usd_usdgeom_basiscurves>().member<TArray<FVector3f>>(MEMBER(usd_usdgeom_basiscurves::Points)).add(flecs::OnInstantiate, flecs::Inherit);

		world.component<nlsfb_class>()
			.member<FString>(MEMBER(nlsfb_class::Code))
			.member<FString>(MEMBER(nlsfb_class::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);
#pragma endregion
	}

	void IFCFeature::CreateSystems(flecs::world& world) {
		world.system<>("TriggerActionAddLayers")
			.with<Operation>(flecs::Wildcard)
			.with<Layer>()
			.with<Action>().id_flags(flecs::TOGGLE).with<Action>()
			.each([&world](flecs::entity action) {
			action.disable<Action>();
			if (!action.has(Operation::Add)) return;
			auto layers = Assets::SelectFiles("Load IFC files", FPaths::ProjectContentDir(), TEXT("IFC 5 Files (*.ifcx)|*.ifcx"));
			LoadIFCFiles(world, layers);
				});
	}
}