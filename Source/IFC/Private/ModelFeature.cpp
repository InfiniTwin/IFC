// Fill out your copyright notice in the Description page of Project Settings.


#include "ModelFeature.h"
#include "AttributeFeature.h"
#include "MeshSubsystem.h"
#include "MaterialSubsystem.h"
#include "ISMSubsystem.h"
#include "ECS.h"
#include "IFC.h"
#include "LayerFeature.h"

namespace IFC {
	FTransform ToTransform(const float values[4][4]) {
		FMatrix matrix(
			FPlane(values[0][0], values[0][1], values[0][2], values[0][3]),
			FPlane(values[1][0], values[1][1], values[1][2], values[1][3]),
			FPlane(values[2][0], values[2][1], values[2][2], values[2][3]),
			FPlane(values[3][0], values[3][1], values[3][2], values[3][3])
		);

		// Correct handedness and units
		FVector location = matrix.GetOrigin();
		location.Y = -location.Y;
		location *= TO_CM;

		FTransform transform(matrix);
		transform.SetLocation(location);

		return transform;
	}

	int32 CreateMesh(flecs::world& world, TArray<FVector3f> points, TArray<int32> indices) {
		// Correct handedness and units
		TArray<FVector3f> correctedPoints;
		correctedPoints.Reserve(points.Num());
		for (FVector3f point : points) {
			FVector3f correctPoint(point.X, -point.Y, point.Z);
			correctPoint *= TO_CM;
			correctedPoints.Add(correctPoint);
		}

		UWorld* uWorld = static_cast<UWorld*>(world.get_ctx());
		return uWorld->GetSubsystem<UMeshSubsystem>()->CreateMesh(uWorld, correctedPoints, indices);
	}

	int32 CreateMaterial(flecs::world& world, const FVector4f& rgba, float offset) {
		UWorld* uWorld = static_cast<UWorld*>(world.get_ctx());
		return uWorld->GetSubsystem<UMaterialSubsystem>()->CreateMaterial(uWorld, rgba, offset);
	}

	int32 FindMaterial(flecs::world& world, flecs::entity ifcObject) {
		auto attributeRelationship = world.try_get<AttributeRelationship>()->Value;

		for (flecs::entity current = ifcObject; current.is_valid(); current = current.parent())
			for (int32_t i = 0;; i++) {
				flecs::entity attributes = current.target(attributeRelationship, i);
				if (!attributes.is_valid())
					break;

				int32 matId = INDEX_NONE;
				attributes.children([&](flecs::entity attribute) {
					if (attribute.has<Material>())
						matId = attribute.try_get<Material>()->Value;
				});

				if (matId != INDEX_NONE)
					return matId;
			}
		return INDEX_NONE;
		//auto defaultMaterial = world.entity();
		//defaultMaterial.set<Owner>({});
		//defultMaterial.set<Material>({ 0 }); // Default material

		//ifcObject.target(attributeRelationship, 0).set<Material>({ 0 }); // Default material
		//return 0;
	}

	void ModelFeature::CreateComponents(flecs::world& world) {
		using namespace ECS;

		world.component<Position>().member<FVector>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Rotation>().member<FRotator>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Scale>().member<FVector>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);

		world.component<Mesh>().member<int32>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<ISM>().member<uint64>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);

		world.component<Material>().member<int32>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
	}

	void ModelFeature::CreateObservers(flecs::world& world) {
		world.observer<>("CreateISMOnIfcObjectCreation")
			.with<IfcObject>()
			.event(flecs::OnAdd)
			.each([&](flecs::entity ifcObject) {
			auto attributeRelationship = world.try_get<AttributeRelationship>()->Value;
			int32 meshId = INDEX_NONE;
			int32_t i = 0;
			while (flecs::entity attributes = ifcObject.target(attributeRelationship, i++))
				attributes.children([&](flecs::entity attribute) {
				if (attribute.has<Mesh>())
					meshId = attribute.try_get<Mesh>()->Value;
			});
			if (meshId == INDEX_NONE)
				return;

			int32 materialId = FindMaterial(world, ifcObject);
			if (materialId == INDEX_NONE)
				return;

			FTransform worldTransform = FTransform::Identity;
			flecs::entity current = ifcObject;
			while (current.is_valid()) {
				FVector	position = FVector::ZeroVector;
				FRotator rotation = FRotator::ZeroRotator;
				FVector scale = FVector::OneVector;

				i = 0;
				while (flecs::entity attributes = current.target(attributeRelationship, i++)) {
					attributes.children([&](flecs::entity attribute) {
						if (attribute.has<Position>())
							position = attribute.try_get<Position>()->Value;
						if (attribute.has<Rotation>())
							rotation = attribute.try_get<Rotation>()->Value;
						if (attribute.has<Scale>())
							scale = attribute.try_get<Scale>()->Value;
					});
				}

				FTransform localTransform(rotation, position, scale);
				worldTransform = localTransform * worldTransform;
				current = current.parent();
			}

			UWorld* uWorld = static_cast<UWorld*>(world.get_ctx());
			ifcObject.set<ISM>({ uWorld->GetSubsystem<UISMSubsystem>()->CreateISM(
				uWorld,
				meshId,
				materialId,
				worldTransform.GetLocation(),
				worldTransform.Rotator(),
				worldTransform.GetScale3D())
				});
		});

		world.observer<Material>("RemoveMaterial")
			.event(flecs::OnRemove)
			.each([&](flecs::entity entity, Material& material) {
			static_cast<UWorld*>(world.get_ctx())
				->GetSubsystem<UMaterialSubsystem>()->Release(material.Value);
		});

		world.observer<ISM>("RemoveISM")
			.event(flecs::OnRemove)
			.each([&](flecs::entity entity, ISM& ism) {
			UWorld* uWorld = static_cast<UWorld*>(world.get_ctx());
			uWorld->GetSubsystem<UISMSubsystem>()->DestroyAll(uWorld);
		});
	}

	void ModelFeature::Initialize(flecs::world& world) {
		CreateMaterial(world, FVector4f(1, 1, 1, 1), true); // Default material
	}
}