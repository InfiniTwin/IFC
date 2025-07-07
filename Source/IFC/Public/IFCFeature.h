// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <flecs.h>

namespace IFC {
	struct IFCFeature {
		static void RegisterComponents(flecs::world& world);
	};

	// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/ifc@v5a.ifcx
	struct DiffuseColor { FLinearColor Value; };
	struct Class { FString Code, Uri; };
	struct Material { FString Code, Uri; };

	// https://ifcx.dev/@standards.buildingsmart.org/ifc/core/prop@v5a.ifcx
	struct IsExternal {};
	struct Volume { float Value; };
	struct Height { float Value; };
	struct Station { float Value; };
	struct TypeName { FString Value; };

	// https://ifcx.dev/@standards.buildingsmart.org/ifc/ifc-mat/prop@v1.0.0.ifcx
	struct StrengthClass { FString Value; };
	struct MoistureContent { float Value; };
	struct MassDensity { float Value; };
	struct GWP { float A1_A3, A4, A5, C2, C3, D; };

	// https://ifcx.dev/@openusd.org/usd@v1.ifcx
	struct Mesh {
		TArray<int> FaceVertexIndices;
		TArray<FVector3f> Points;
	};
	struct Xformop { TArray<FVector4f> Transform; };
}