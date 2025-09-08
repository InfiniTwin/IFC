// Fill out your copyright notice in the Description page of Project Settings.


#include "ModelFeature.h"
#include "AttributeFeature.h"
#include "MeshSubsystem.h"
#include "MaterialSubsystem.h"
#include "ISMSubsystem.h"
#include "IFC.h"
#include "Assets.h"
#include "ECS.h"

namespace IFC {
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
			int32 meshId = INDEX_NONE;
			int32_t index = 0;
			while (flecs::entity attributes = ifcObject.target(world.try_get<AttributeRelationship>()->Value, index++)) {
				attributes.children([&](flecs::entity attribute) {
					if (attribute.has<Mesh>())
						meshId = attribute.try_get<Mesh>()->Value;
					});
			}
			if (meshId == INDEX_NONE) return;

			FTransform worldTransform = FTransform::Identity;
			flecs::entity current = ifcObject;
			while (current.is_alive()) {
				FVector position = FVector::ZeroVector;
				FRotator rotation = FRotator::ZeroRotator;
				FVector scale = FVector::OneVector;

				current.target<Attribute>().children([&](flecs::entity attribute) {
					if (attribute.has<Position>())
						position = attribute.try_get<Position>()->Value;
					if (attribute.has<Rotation>())
						rotation = attribute.try_get<Rotation>()->Value;
					if (attribute.has<Scale>())
						scale = attribute.try_get<Scale>()->Value;
					});

				FTransform localTransform(rotation, position, scale);
				worldTransform = localTransform * worldTransform;
				current = current.parent();
			}

			UWorld* uWorld = static_cast<UWorld*>(world.get_ctx());
			ifcObject.set<ISM>({ uWorld->GetSubsystem<UISMSubsystem>()->CreateISM(
				uWorld,
				meshId,
				worldTransform.GetLocation(),
				worldTransform.Rotator(),
				worldTransform.GetScale3D())
				});
				});
	}

	FTransform ToTransform(const float values[4][4]) {
		FMatrix matrix(
			FPlane(values[0][0], values[0][1], values[0][2], values[0][3]),
			FPlane(values[1][0], values[1][1], values[1][2], values[1][3]),
			FPlane(values[2][0], values[2][1], values[2][2], values[2][3]),
			FPlane(values[3][0], values[3][1], values[3][2], values[3][3])
		);

		FTransform transform(matrix);
		transform.SetLocation(transform.GetLocation() * TO_CM);
		return transform;
	}

	int32 CreateMesh(flecs::world& world, TArray<FVector3f> points, TArray<int32> indices) {
		TArray<FVector3f> scaledPoints = points;
		for (FVector3f& point : scaledPoints) point *= TO_CM;
		
		UWorld* uWorld = static_cast<UWorld*>(world.get_ctx());
		return uWorld->GetSubsystem<UMeshSubsystem>()->CreateMesh(uWorld, scaledPoints, indices);
	}

	int32 CreateMaterial(flecs::world& world, const FVector4f& rgba) {
		UWorld* uWorld = static_cast<UWorld*>(world.get_ctx());
		return uWorld->GetSubsystem<UMaterialSubsystem>()->CreateMaterial(uWorld, rgba);
	}
}