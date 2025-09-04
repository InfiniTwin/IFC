// Fill out your copyright notice in the Description page of Project Settings.


#include "AttributeFeature.h"
#include "IFC.h"
#include "Assets.h"
#include "ECS.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/writer.h"

namespace IFC {
	void AttributeFeature::CreateComponents(flecs::world& world) {
		using namespace ECS;
		world.component<Attribute>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<Value>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
	}

	using namespace rapidjson;

	FString JsonValueToString(const rapidjson::Value& value) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		value.Accept(writer);
		return UTF8_TO_TCHAR(buffer.GetString());
	}

	static bool TryExtractRefString(const rapidjson::Value& value, FString& out) {
		if (!value.IsObject()) return false;
		auto it = value.FindMember("ref");
		if (it == value.MemberEnd() || !it->value.IsString()) return false;
		out = UTF8_TO_TCHAR(it->value.GetString());
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

	static FString GetAttributeNestedNameAndValue(const rapidjson::Value& v) {
		if (!v.IsObject())
			return TEXT("");

		FString result;
		for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it) {
			const FString childName = UTF8_TO_TCHAR(it->name.GetString());
			const FString childValue = (
				childName == "faceVertexIndices" || 
				childName == "points")
				? "" : GetValueAsString(it->value);

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

		FString prefab = FString::Printf(TEXT("prefab %s {\n"), *path);
		prefab += FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(IfcObject)));

		const rapidjson::Value& attributes = object[ATTRIBUTES];
		for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
			const FString nameAndOwner = UTF8_TO_TCHAR(itr->name.GetString());
			FString owner, name;
			nameAndOwner.Split(ATTRIBUTE_SEPARATOR, &owner, &name);

			const rapidjson::Value& attrValue = itr->value;

			prefab += FString::Printf(TEXT("\t_ : %s {%s%s%s\n\t}\n"),
				*owner,
				*FString::Printf(TEXT("\n\t\t%s"), UTF8_TO_TCHAR(COMPONENT(Attribute))),
				*GetAttributeNameAndValue(name, attrValue),
				*GetAttributeNestedNameAndValue(attrValue));
		}

		prefab += "\n}\n";

		return MakeTuple(path, prefab);
	}
}