// Fill out your copyright notice in the Description page of Project Settings.


#include "AttributeFeature.h"
#include "IFC.h"
#include "LayerFeature.h"
#include "ECS.h"
#include "ModelFeature.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"

namespace IFC {
	float defaultOpacity = 1;
	float defaultOffset = 0;
	float ifcSpaceOffset = 0.01;

	void AttributeFeature::CreateComponents(flecs::world& world) {
		using namespace ECS;
		world.component<AttributesRelationship>().add(flecs::Singleton);

		world.component<Attribute>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Value>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);

		// Entities
		world.component<Alignment>();
		world.component<AlignmentCant>();
		world.component<AlignmentHorizontal>();
		world.component<AlignmentSegment>();
		world.component<AlignmentVertical>();
		world.component<Boiler>();
		world.component<Building>();
		world.component<BuildingStorey>();
		world.component<DistributionPort>();
		world.component<PipeFitting>();
		world.component<PipeSegment>();
		world.component<Project>();
		world.component<Railway>();
		world.component<Referent>();
		world.component<SanitaryTerminal>();
		world.component<Signal>();
		world.component<Site>();
		world.component<Slab>();
		world.component<Space>();
		world.component<Valve>();
		world.component<Wall>();
		world.component<Window>();

		// Relationships
		world.component<SpaceBoundary>();
		world.component<RelatedElement>();
		world.component<RelatingSpace>();
		world.component<PartOfSystem>();
		world.component<ConnectsTo>();

		// Enums
		world.component<FlowDirection>().add(flecs::Exclusive);
	}

	void AttributeFeature::Initialize(flecs::world& world) {
		world.set<AttributesRelationship>({ world.entity(ATTRIBUTES_RELATIONSHIP) });
	}

	TArray<flecs::entity> GetAttributes(flecs::world& world, flecs::entity ifcObject) {
		TMap<FString, flecs::entity> uniqueAttributes;
		int32_t index = 0;
		while (flecs::entity attributes = ifcObject.target(world.try_get<AttributesRelationship>()->Value, index++)) {
			attributes.children([&](flecs::entity attribute) {
				if (!attribute.has<Attribute>()) return;

				FString key = FString::Printf(TEXT("%llu|%s|%s"),
					(uint64)attribute.id(),
					*attribute.try_get<Name>()->Value,
					*attribute.try_get<Owner>()->Value);

				uniqueAttributes.Add(key, attribute);
			});
		}

		TArray<flecs::entity> result;
		uniqueAttributes.GenerateValueArray(result);
		return result;
	}

	using namespace rapidjson;

	bool HasAttribute(const TSet<FString>& attributes, const FString& name) {
		for (const FString& attribute : attributes)
			if (name.Contains(attribute))
				return true;
		return false;
	}

	FString JsonValueToString(const rapidjson::Value& value) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		value.Accept(writer);
		return UTF8_TO_TCHAR(buffer.GetString());
	}

	static bool TryExtractRefString(const rapidjson::Value& value, FString& out) {
		if (!value.IsObject()) return false;
		auto ref = value.FindMember("ref");
		if (ref == value.MemberEnd() || !ref->value.IsString()) return false;
		out = UTF8_TO_TCHAR(ref->value.GetString());
		return true;
	}

	static FString GetValueAsString(const rapidjson::Value& value) {
		if (value.IsString())
			return ECS::CleanCode(UTF8_TO_TCHAR(value.GetString()));

		return ECS::CleanCode(JsonValueToString(value));
	}

	static FString GetAttributeNestedNameAndValue(const rapidjson::Value& value) {
		if (!value.IsObject())
			return TEXT("");

		FString result;
		for (auto child = value.MemberBegin(); child != value.MemberEnd(); ++child) {
			const FString childName = UTF8_TO_TCHAR(child->name.GetString());
			const FString childValue = GetValueAsString(child->value);

			result += TEXT("\n\t\t_ {");
			result += FString::Printf(TEXT("\n\t\t\t%s: {\"%s\"}"),
				UTF8_TO_TCHAR(COMPONENT(Name)),
				*childName);
			result += FString::Printf(TEXT("\n\t\t\t%s: {\"%s\"}"),
				UTF8_TO_TCHAR(COMPONENT(Value)),
				*childValue);
			result += TEXT("\n\t\t}");
		}
		return result;
	}

	static FString GetAttributeNameAndValue(const FString& name, const rapidjson::Value& value) {
		FString result = FString::Printf(TEXT("\n\t\t%s: {\"%s\"}"),
			UTF8_TO_TCHAR(COMPONENT(Name)),
			*name);

		if (!value.IsObject())
			result += FString::Printf(TEXT("\n\t\t%s: {\"%s\"}"),
				UTF8_TO_TCHAR(COMPONENT(Value)),
				*GetValueAsString(value));

		return result;
	}

	static FString GetAttributeEntity(const FString& name, const rapidjson::Value& value) {
		FString entity = FString::Printf(TEXT("\n\t\t%s"), UTF8_TO_TCHAR(COMPONENT(Attribute)));
		entity += GetAttributeNameAndValue(name, value);
		entity += GetAttributeNestedNameAndValue(value);
		return entity;
	}

	static FString ProcessRelationship(const FString& relationship, const rapidjson::Value& value) {
		FString result;

		auto addRef = [&](const FString& refStr) {
			const FString target = IFC::Scope() + TEXT(".") + MakeId(refStr);
			result += FString::Printf(TEXT("\n\t\t(%s, %s)"),
				*relationship,
				*target);
		};

		if (value.IsArray() && value.Size() > 0) { // Array of refs (only first element)
			const auto& refObj = value[0];
			if (refObj.HasMember("ref") && refObj["ref"].IsString())
				addRef(UTF8_TO_TCHAR(refObj["ref"].GetString()));
		} else if (value.IsObject()) { // Single ref
			FString ref;
			if (TryExtractRefString(value, ref))
				addRef(ref);
		}

		return result;
	}

	TTuple<FString, bool> ProcessAttribute(flecs::world& world, const FString& name, const rapidjson::Value& value, const rapidjson::Value& attributes) {
		if (name == ATTRIBUTE_XFORMOP) {
			const rapidjson::Value& transformData = value[ATTRIBUTE_TRANSFROM];
			float values[4][4];
			for (int rowIndex = 0; rowIndex < 4; ++rowIndex) {
				const rapidjson::Value& transformRowData = transformData[rowIndex];
				for (int columnIndex = 0; columnIndex < 4; ++columnIndex)
					values[rowIndex][columnIndex] = static_cast<float>(transformRowData[columnIndex].GetDouble());
			}

			FTransform transform = ToTransform(values);
			const FVector position = transform.GetLocation();
			const FRotator rotation = transform.Rotator();
			const FVector scale = transform.GetScale3D();

			FString result = FString::Printf(TEXT("\n\t\t%s: {{%.6f, %.6f, %.6f}}"),
				UTF8_TO_TCHAR(COMPONENT(Position)),
				position.X, position.Y, position.Z);

			result += FString::Printf(TEXT("\n\t\t%s: {{%.6f, %.6f, %.6f}}"),
				UTF8_TO_TCHAR(COMPONENT(Rotation)),
				rotation.Pitch, rotation.Yaw, rotation.Roll);

			result += FString::Printf(TEXT("\n\t\t%s: {{%.6f, %.6f, %.6f}}"),
				UTF8_TO_TCHAR(COMPONENT(Scale)),
				scale.X, scale.Y, scale.Z);

			// Create Transform Attribute
			rapidjson::Document transformAttribute(rapidjson::kObjectType);
			auto& allocator = transformAttribute.GetAllocator();
			rapidjson::Value transformObject(rapidjson::kObjectType);

			auto addVector = [&](const char* key, float x, float y, float z) {
				rapidjson::Value arr(rapidjson::kArrayType);
				arr.PushBack(x, allocator).PushBack(y, allocator).PushBack(z, allocator);
				transformObject.AddMember(rapidjson::Value(key, allocator), arr, allocator);
			};

			addVector(COMPONENT(Position), position.X, position.Y, position.Z);
			addVector(COMPONENT(Rotation), rotation.Pitch, rotation.Yaw, rotation.Roll);
			addVector(COMPONENT(Scale), scale.X, scale.Y, scale.Z);

			result += GetAttributeEntity(ATTRIBUTE_TRANSFROM, transformObject);

			return MakeTuple(result, false);
		}

		if (name == ATTRIBUTE_MESH) {
			const rapidjson::Value& indicesData = value[MESH_INDICES];
			const rapidjson::Value& pointsData = value[MESH_POINTS];

			TArray<int32> indices;
			indices.Reserve(static_cast<int32>(indicesData.Size()));
			for (auto& index : indicesData.GetArray())
				if (index.IsInt())
					indices.Add(index.GetInt());

			TArray<FVector3f> points;
			points.Reserve(static_cast<int32>(pointsData.Size()));
			for (auto& point : pointsData.GetArray())
				points.Add(FVector3f(
					static_cast<float>(point[0].GetDouble()),
					static_cast<float>(point[1].GetDouble()),
					static_cast<float>(point[2].GetDouble())));

			return MakeTuple(FString::Printf(TEXT("\n\t\t%s: {%d}"),
				UTF8_TO_TCHAR(COMPONENT(Mesh)),
				CreateMesh(world, points, indices)),
				false);
		}

		if (name == ATTRIBUTE_DIFFUSECOLOR) {
			float opacity = defaultOpacity;
			float offset = defaultOffset;
			for (auto attributeOpacity = attributes.MemberBegin(); attributeOpacity != attributes.MemberEnd(); ++attributeOpacity)
				if (FCStringAnsi::Strstr(attributeOpacity->name.GetString(), ATTRIBUTE_OPACITY) != nullptr) {
					opacity = static_cast<float>(attributeOpacity->value.GetDouble());
					break;
				}

			for (auto attributeIfcClass = attributes.MemberBegin(); attributeIfcClass != attributes.MemberEnd(); ++attributeIfcClass) {
				if (FCStringAnsi::Strstr(attributeIfcClass->name.GetString(), ATTRIBUTE_IFC_CLASS) != nullptr
					&& FCStringAnsi::Strcmp(attributeIfcClass->value[IFC_CLASS_CODE].GetString(), IFC_SPACE) == 0) {
					offset = ifcSpaceOffset;
					break;
				}
			}

			FVector4f rgba(
				static_cast<float>(value[0].GetDouble()),
				static_cast<float>(value[1].GetDouble()),
				static_cast<float>(value[2].GetDouble()),
				opacity);

			return MakeTuple(FString::Printf(TEXT("\n\t\t%s: {%d}"),
				UTF8_TO_TCHAR(COMPONENT(Material)),
				CreateMaterial(world, rgba, offset)),
				false);
		}

		if (name == ATTRIBUTE_VISIBILITY) {
			bool invisible = value.HasMember(VISIBILITY_VISIBILITY) && value[VISIBILITY_VISIBILITY] == VISIBILITY_INVISIBLE;
			FVector4f rgba(1, 1, 1, invisible ? 0 : 1);

			return MakeTuple(FString::Printf(TEXT("\n\t\t%s: {%d}"),
				UTF8_TO_TCHAR(COMPONENT(Material)),
				CreateMaterial(world, rgba, defaultOffset)),
				false);
		}

		if (name == ATTRIBUTE_SPACE_BOUNDARY) {
			FString result = FString::Printf(TEXT("\n\t\t%s"), UTF8_TO_TCHAR(COMPONENT(SpaceBoundary)));
			result += ProcessRelationship(COMPONENT(RelatedElement), value[RELATED_ELEMENT]);
			result += ProcessRelationship(COMPONENT(RelatingSpace), value[RELATING_SPACE]);
			return MakeTuple(result, true);
		}

		if (name == PART_OF_SYSTEM)
			return MakeTuple(ProcessRelationship(COMPONENT(PartOfSystem), value), true);

		if (name == CONNECTS_TO)
			return MakeTuple(ProcessRelationship(COMPONENT(ConnectsTo), value), true);

		if (const FString* enumAttribute = EnumAttributes.Find(name))
			return MakeTuple(FString::Printf(TEXT("\n\t\t(%s, %s)"),
				**enumAttribute,
				UTF8_TO_TCHAR(value.GetString())),
				false);

		if (name == ATTRIBUTE_IFC_CLASS) {
			FString result = GetAttributeEntity(ATTRIBUTE_IFC_CLASS, value);
			FString entity = UTF8_TO_TCHAR(value[IFC_CLASS_CODE].GetString());
			result += FString::Printf(TEXT("\n\t\t%s"), *entity.RightChop(3));
			return MakeTuple(result, false);
		}

		return MakeTuple("", false);
	}

	TTuple<FString, FString, FString> GetAttributes(flecs::world& world, const rapidjson::Value& object, const FString& objectPath) {
		if (!object.HasMember(ATTRIBUTES_KEY) || !object[ATTRIBUTES_KEY].IsObject())
			return MakeTuple(FString(), FString(), FString());

		FString path = IFC::Scope() + "." + ATTRIBUTES_KEY + objectPath;
		FString attributes = FString::Printf(TEXT("%s {\n"), *path);
		FString relationships = attributes;
		attributes += FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(IfcObject)));

		bool hasRelationships = false;

		const rapidjson::Value& attributesObject = object[ATTRIBUTES_KEY];
		for (auto attribute = attributesObject.MemberBegin(); attribute != attributesObject.MemberEnd(); ++attribute) {
			const FString nameAndOwner = UTF8_TO_TCHAR(attribute->name.GetString());
			FString owner, name;
			nameAndOwner.Split(ATTRIBUTE_SEPARATOR, &owner, &name);

			if (HasAttribute(ExcludeAttributes, name))
				continue;

			FString entities = "";

			const rapidjson::Value& value = attribute->value;

			TTuple <FString, bool> data = ProcessAttribute(world, name, value, attributesObject);
			FString attributeValue = data.Get<0>();
			bool isRelationship = data.Get<1>();

			if (!attributeValue.IsEmpty()) // Processed attribute
				entities += attributeValue;
			else
				entities += GetAttributeEntity(name, value);

			FString entity = FString::Printf(TEXT("\t_ : %s {%s\n\t}\n"),
				*owner,
				*entities);

			if (!isRelationship)
				attributes += entity;
			else {
				relationships += entity;
				hasRelationships = true;
			}
		}

		attributes += "}\n";
		relationships += "}\n";

		return MakeTuple(path, attributes, hasRelationships ? relationships : "");
	}
}