// Fill out your copyright notice in the Description page of Project Settings.


#include "AttributeFeature.h"
#include "IFC.h"
#include "Assets.h"
#include "ECS.h"
#include "ModelFeature.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/writer.h"

namespace IFC {
	void AttributeFeature::CreateComponents(flecs::world& world) {
		using namespace ECS;
		world.component<Attribute>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Value>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<AttributeRelationship>().add(flecs::Singleton);
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

	FString ProcessAttribute(flecs::world& world, const FString& name, const rapidjson::Value& value, const rapidjson::Value& attributes) {
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

			return result;
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

			return FString::Printf(
				TEXT("\n\t\t%s: {%d}"),
				UTF8_TO_TCHAR(COMPONENT(Mesh)),
				CreateMesh(world, points, indices));
		}

		if (name == ATTRIBUTE_DIFFUSECOLOR) {
			float opacity = 0;
			for (auto attributeMaterial = attributes.MemberBegin(); attributeMaterial != attributes.MemberEnd(); ++attributeMaterial)
				if (FCStringAnsi::Strstr(attributeMaterial->name.GetString(), ATTRIBUTE_MATERIAL) != nullptr) {
					opacity = 1;
					for (auto attributeOpacity = attributes.MemberBegin(); attributeOpacity != attributes.MemberEnd(); ++attributeOpacity)
						if (FCStringAnsi::Strstr(attributeOpacity->name.GetString(), ATTRIBUTE_OPACITY) != nullptr) {
							opacity = static_cast<float>(attributeOpacity->value.GetDouble());
							break;
						}
				}

			FVector4f rgba(
				static_cast<float>(value[0].GetDouble()),
				static_cast<float>(value[1].GetDouble()),
				static_cast<float>(value[2].GetDouble()),
				opacity);

			return FString::Printf(
				TEXT("\n\t\t%s: {%d}"),
				UTF8_TO_TCHAR(COMPONENT(Material)),
				CreateMaterial(world, rgba));
		}

		if (name == ATTRIBUTE_VISIBILITY) {
			bool invisible = value.HasMember(VISIBILITY_VISIBILITY) && value[VISIBILITY_VISIBILITY] == VISIBILITY_INVISIBLE;
			FVector4f rgba(1, 1, 1, invisible ? 0 : 1);

			return FString::Printf(
				TEXT("\n\t\t%s: {%d}"),
				UTF8_TO_TCHAR(COMPONENT(Material)),
				CreateMaterial(world, rgba));
		}

		return "";
	}

	TTuple<FString, FString> GetAttributes(flecs::world& world, const rapidjson::Value& object, const FString& objectPath) {
		if (!object.HasMember(ATTRIBUTES_KEY) || !object[ATTRIBUTES_KEY].IsObject())
			return MakeTuple(FString(), FString());

		FString path = IFC::Scope() + "." + ATTRIBUTES_KEY + objectPath;

		FString attributesEntity = FString::Printf(TEXT("%s {\n"), *path);
		attributesEntity += FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(IfcObject)));

		const rapidjson::Value& attributes = object[ATTRIBUTES_KEY];
		for (auto attribute = attributes.MemberBegin(); attribute != attributes.MemberEnd(); ++attribute) {
			const FString nameAndOwner = UTF8_TO_TCHAR(attribute->name.GetString());
			FString owner, name;
			nameAndOwner.Split(ATTRIBUTE_SEPARATOR, &owner, &name);

			if (HasAttribute(ExcludeAttributes, name))
				continue;

			FString attributeEntities = "";

			const rapidjson::Value& value = attribute->value;

			FString processedAttribute = ProcessAttribute(world, name, value, attributes);

			if (!processedAttribute.IsEmpty())
				attributeEntities += processedAttribute;
			else
				attributeEntities += GetAttributeEntity(name, value);

			attributesEntity += FString::Printf(TEXT("\t_ : %s {%s\n\t}\n"),
				*owner,
				*attributeEntities);
		}

		attributesEntity += "\n}\n";

		return MakeTuple(path, attributesEntity);
	}
}