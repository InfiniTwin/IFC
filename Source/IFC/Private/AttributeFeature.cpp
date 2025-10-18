// Fill out your copyright notice in the Description page of Project Settings.


#include "AttributeFeature.h"
#include "IFC.h"
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
		world.component<AttributeRelationship>().add(flecs::Singleton);

		world.component<Attribute>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Value>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);

		// Entities
		world.component<Alignment>();
		world.component<AlignmentCant>();
		world.component<AlignmentHorizontal>();
		world.component<AlignmentSegment>();
		world.component<AlignmentVertical>();
		world.component<Building>();
		world.component<BuildingStorey>();
		world.component<Project>();
		world.component<Railway>();
		world.component<Referent>();
		world.component<Signal>();
		world.component<Site>();
		world.component<Space>();
		world.component<Wall>();
		world.component<Window>();

		// Relationships
		world.component<SpaceBoundary>();
		world.component<RelatedElement>();
		world.component<RelatingSpace>();
		world.component<Segment>();
	}

	void AttributeFeature::Initialize(flecs::world& world) {
		world.set<AttributeRelationship>({ world.entity() });
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
		// 1) Plain string
		if (value.IsString())
			return ECS::CleanCode(UTF8_TO_TCHAR(value.GetString()));

		// 2) Single { "ref": "..." }
		if (value.IsObject() && value.MemberCount() == 1) {
			FString refString;
			if (TryExtractRefString(value, refString))
				return ECS::CleanCode(refString);
		}

		// 3) Array of { "ref": "..." }
		if (value.IsArray()) {
			TArray<FString> refs;
			refs.Reserve(static_cast<int32>(value.Size()));

			for (auto& elem : value.GetArray()) {
				FString refString;
				if (TryExtractRefString(elem, refString))
					refs.Add(ECS::CleanCode(refString));
			}

			if (refs.Num() > 0) {
				TArray<FString> quoted;
				quoted.Reserve(refs.Num());
				for (const FString& s : refs)
					quoted.Add(FString::Printf(TEXT("%s"), *s));
				return FString::Printf(TEXT("%s"), *FString::Join(quoted, TEXT(",")));
			}
		}

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
			result += FString::Printf(TEXT("\n\t\t\t%s: {\"%s\"}"), UTF8_TO_TCHAR(COMPONENT(Name)), *childName);
			result += FString::Printf(TEXT("\n\t\t\t%s: {\"%s\"}"), UTF8_TO_TCHAR(COMPONENT(Value)), *childValue);
			result += TEXT("\n\t\t}");
		}
		return result;
	}

	static FString GetAttributeNameAndValue(const FString& name, const rapidjson::Value& value) {
		FString result = FString::Printf(TEXT("\n\t\t%s: {\"%s\"}"), UTF8_TO_TCHAR(COMPONENT(Name)), *name);

		if (!value.IsObject())
			result += FString::Printf(TEXT("\n\t\t%s: {\"%s\"}"), UTF8_TO_TCHAR(COMPONENT(Value)), *GetValueAsString(value));

		return result;
	}

	FString GetAttributeEntity(const FString& name, const rapidjson::Value& value) {
		FString entity = FString::Printf(TEXT("\n\t\t%s"), UTF8_TO_TCHAR(COMPONENT(Attribute)));
		entity += GetAttributeNameAndValue(name, value);
		entity += GetAttributeNestedNameAndValue(value);
		return entity;
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

			FString result = FString::Printf(
				TEXT("\n\t\t%s: {{%.6f, %.6f, %.6f}}"),
				UTF8_TO_TCHAR(COMPONENT(Position)),
				position.X, position.Y, position.Z);

			result += FString::Printf(
				TEXT("\n\t\t%s: {{%.6f, %.6f, %.6f}}"),
				UTF8_TO_TCHAR(COMPONENT(Rotation)),
				rotation.Pitch, rotation.Yaw, rotation.Roll);

			result += FString::Printf(
				TEXT("\n\t\t%s: {{%.6f, %.6f, %.6f}}"),
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

			return MakeTuple(FString::Printf(
				TEXT("\n\t\t%s: {%d}"),
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

			return MakeTuple(FString::Printf(
				TEXT("\n\t\t%s: {%d}"),
				UTF8_TO_TCHAR(COMPONENT(Material)),
				CreateMaterial(world, rgba, offset)),
				false);
		}

		if (name == ATTRIBUTE_VISIBILITY) {
			bool invisible = value.HasMember(VISIBILITY_VISIBILITY) && value[VISIBILITY_VISIBILITY] == VISIBILITY_INVISIBLE;
			FVector4f rgba(1, 1, 1, invisible ? 0 : 1);

			return MakeTuple(FString::Printf(
				TEXT("\n\t\t%s: {%d}"),
				UTF8_TO_TCHAR(COMPONENT(Material)),
				CreateMaterial(world, rgba, defaultOffset)),
				false);
		}

		if (name == ATTRIBUTE_SPACE_BOUNDARY) {
			const rapidjson::Value* relatedElement = &value[RELATED_ELEMENT];
			const rapidjson::Value* relatingSpace = &value[RELATING_SPACE];

			FString result = FString::Printf(TEXT("\n\t\t%s"), UTF8_TO_TCHAR(COMPONENT(SpaceBoundary)));

			if (relatedElement) {
				FString ref;
				if (TryExtractRefString(*relatedElement, ref)) {
					const FString target = IFC::Scope() + TEXT(".") + MakeId(ref);
					result += FString::Printf(
						TEXT("\n\t\t(%s, %s)"),
						UTF8_TO_TCHAR(COMPONENT(RelatedElement)),
						*target);
				}
			}

			if (relatingSpace) {
				FString ref;
				if (TryExtractRefString(*relatingSpace, ref)) {
					const FString target = IFC::Scope() + TEXT(".") + MakeId(ref);
					result += FString::Printf(
						TEXT("\n\t\t(%s, %s)"),
						UTF8_TO_TCHAR(COMPONENT(RelatingSpace)),
						*target);
				}
			}

			return MakeTuple(result, true);
		}

		if (name == ATTRIBUTE_IFC_CLASS) {
			FString result = GetAttributeEntity(ATTRIBUTE_TRANSFROM, value);
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