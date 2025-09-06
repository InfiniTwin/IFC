// Fill out your copyright notice in the Description page of Project Settings.


#include "ModelFeature.h"
#include "AttributeFeature.h"
#include "ISMSubsystem.h"
#include "IFC.h"
#include "Assets.h"
#include "ECS.h"
#include "MeshSubsystem.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/writer.h"

namespace IFC {
	void ModelFeature::CreateComponents(flecs::world& world) {
		using namespace ECS;

		world.component<Position>().member<FVector>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Rotation>().member<FRotator>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Scale>().member<FVector>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);

		world.component<Mesh>().member<int32>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<ISM>().member<uint64>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
	}

	void ModelFeature::CreateObservers(flecs::world& world) {
		world.observer<>("CreateISMOnIfcObjectCreation")
			.with<IfcObject>().filter()
			.with<Attribute>(flecs::Wildcard)
			.event(flecs::OnAdd)
			.each([&](flecs::entity ifcObject) {
			int32 meshId = INDEX_NONE;
			ifcObject.target<Attribute>().children([&](flecs::entity attribute) {
				if (attribute.has<Mesh>()) 
					meshId = attribute.try_get<Mesh>()->Value;
				});
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

	int32 CreateMesh(flecs::world& world, const rapidjson::Value& mesh) {
		const rapidjson::Value& indicesData = mesh[MESH_INDICES];
		const rapidjson::Value& pointsData = mesh[MESH_POINTS];

		TArray<int32> indices;
		indices.Reserve(static_cast<int32>(indicesData.Size()));
		for (auto& index : indicesData.GetArray())
			if (index.IsInt())
				indices.Add(index.GetInt());

		TArray<FVector3f> points;
		points.Reserve(static_cast<int32>(pointsData.Size()));
		for (auto& point : pointsData.GetArray())
			if (point.IsArray() && point.Size() == 3) {
				float x = static_cast<float>(point[0].GetDouble());
				float y = static_cast<float>(point[1].GetDouble());
				float z = static_cast<float>(point[2].GetDouble());
				points.Add(FVector3f(x, y, z));
			}

		if (indices.Num() > 0 && points.Num() > 0) {
			UWorld* uWorld = static_cast<UWorld*>(world.get_ctx());
			return uWorld->GetSubsystem<UMeshSubsystem>()->CreateMesh(uWorld, points, indices);
		}

		return INDEX_NONE;
	}

	FTransform ToTransform(const rapidjson::Value& xformop) {
		const rapidjson::Value& transformArray = xformop[ATTRIBUTE_TRANSFROM];

		float values[4][4];
		for (int rowIndex = 0; rowIndex < 4; ++rowIndex) {
			const rapidjson::Value& transformRowArray = transformArray[rowIndex];
			for (int columnIndex = 0; columnIndex < 4; ++columnIndex)
				values[rowIndex][columnIndex] = static_cast<float>(transformRowArray[columnIndex].GetDouble());
		}

		FMatrix matrix(
			FPlane(values[0][0], values[0][1], values[0][2], values[0][3]),
			FPlane(values[1][0], values[1][1], values[1][2], values[1][3]),
			FPlane(values[2][0], values[2][1], values[2][2], values[2][3]),
			FPlane(values[3][0], values[3][1], values[3][2], values[3][3])
		);

		FTransform transform(matrix);
		transform.SetLocation(transform.GetLocation() * 100.f);
		return transform;
	}
}