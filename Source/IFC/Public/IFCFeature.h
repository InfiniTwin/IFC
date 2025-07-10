// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <flecs.h>
#include "ECS.h"
#include "ECSCore.h"

namespace IFC {
	struct IFCFeature {
		static void RegisterComponents(flecs::world& world);
	};

	// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/ifc@v5a.ifcx
	struct bsi_ifc_presentation_diffuseColor { FLinearColor Value; };
	struct bsi_ifc_class { FString Code, Uri; };
	struct bsi_ifc_material { FString Code, Uri; };

	// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/prop@v5a.ifcx
	struct bsi_ifc_prop_IsExternal {};
	struct bsi_ifc_prop_Volume { float Value; };
	struct bsi_ifc_prop_Height { float Value; };
	struct bsi_ifc_prop_Station { float Value; };
	struct bsi_ifc_prop_TypeName { FString Value; };

	// https://ifcx.dev/@standards.buildingsmart.org/ifc/ifc-mat/prop@v1.0.0.ifcx
	struct bsi_ifc_mat_prop_StrengthClass { FString Value; };
	struct bsi_ifc_mat_prop_MoistureContent { float Value; };
	struct bsi_ifc_mat_prop_MassDensity { float Value; };
	struct bsi_ifc_mat_prop_GWP { float A1_A3, A4, A5, C2, C3, D; };

	// https://ifcx.dev/@openusd.org/usd@v1.ifcx
	struct usd_usdgeom_mesh {
		TArray<int> FaceVertexIndices;
		TArray<FVector3f> Points;
	};
	struct usd_usdgeom_visibility { FString Value; };
	using namespace ECS;
	struct usd_xformop { TArray<Vector4> Transform; };

	// "https://ifcx.dev/@nlsfb/nlsfb@v1.ifcx"
	struct nlsfb_class { FString Code, Uri; };
}