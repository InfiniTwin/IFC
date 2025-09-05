// Fill out your copyright notice in the Description page of Project Settings.


#include "AttributeFeature.h"
#include "IFC.h"
#include "Assets.h"
#include "ECS.h"
#include "MeshSubsystem.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/writer.h"

namespace IFC {
	void AttributeFeature::CreateComponents(flecs::world& world) {
		using namespace ECS;
		world.component<Attribute>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Value>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Mesh>().member<int32>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);;
	}

	using namespace rapidjson;

	int32 CreateMesh(const rapidjson::Value& value) {
		if (value.IsObject()) {
			const rapidjson::Value* indicesArray = value.HasMember(MESH_INDICES) ? &value[MESH_INDICES] : nullptr;
			const rapidjson::Value* pointsArray = value.HasMember(MESH_POINTS) ? &value[MESH_POINTS] : nullptr;

			if (indicesArray && indicesArray->IsArray() && pointsArray && pointsArray->IsArray()) {
				TArray<int32> faceVertexIndices;
				faceVertexIndices.Reserve(static_cast<int32>(indicesArray->Size()));
				for (auto& elem : indicesArray->GetArray())
					if (elem.IsInt())
						faceVertexIndices.Add(elem.GetInt());

				TArray<FVector3f> points;
				points.Reserve(static_cast<int32>(pointsArray->Size()));
				for (auto& p : pointsArray->GetArray())
					if (p.IsArray() && p.Size() == 3) {
						float x = static_cast<float>(p[0].GetDouble());
						float y = static_cast<float>(p[1].GetDouble());
						float z = static_cast<float>(p[2].GetDouble());
						points.Add(FVector3f(x, y, z));
					}

				if (faceVertexIndices.Num() > 0 && points.Num() > 0) {
					UWorld* worldObj = GWorld;
					return worldObj->GetSubsystem<UMeshSubsystem>()->CreateMesh(worldObj, points, faceVertexIndices);
				}
			}
		}

		return INDEX_NONE;
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

	TTuple<FString, FString> GetAttributes(const rapidjson::Value& object, const FString& objectPath) {
		if (!object.HasMember(ATTRIBUTES) || !object[ATTRIBUTES].IsObject())
			return MakeTuple(FString(), FString());

		FString path = IFC::Scope() + "." + ATTRIBUTES + objectPath;

		FString attributesEntity = FString::Printf(TEXT("%s {\n"), *path);
		attributesEntity += FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(IfcObject)));

		const rapidjson::Value& attributes = object[ATTRIBUTES];
		for (auto attribute = attributes.MemberBegin(); attribute != attributes.MemberEnd(); ++attribute) {
			FString attributeEntities = "";
			bool processed = false;

			const FString nameAndOwner = UTF8_TO_TCHAR(attribute->name.GetString());
			FString owner, name;
			nameAndOwner.Split(ATTRIBUTE_SEPARATOR, &owner, &name);

			const rapidjson::Value& value = attribute->value;

			if (name == ATTRIBUTE_MESH) {
				int32 meshId = CreateMesh(value);
				if (meshId != INDEX_NONE)
					attributeEntities += FString::Printf(TEXT("\n\t\t%s: {%d}"), UTF8_TO_TCHAR(COMPONENT(Mesh)), meshId);
				processed = true;
			}

			if (!processed) {
				attributeEntities += FString::Printf(TEXT("\n\t\t%s"), UTF8_TO_TCHAR(COMPONENT(Attribute)));
				attributeEntities += GetAttributeNameAndValue(name, value);
				attributeEntities += GetAttributeNestedNameAndValue(value);
			}

			attributesEntity += FString::Printf(TEXT("\t_ : %s {%s\n\t}\n"),
				*owner,
				*attributeEntities);
		}

		attributesEntity += "\n}\n";

		return MakeTuple(path, attributesEntity);
	}
}