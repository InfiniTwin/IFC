// Fill out your copyright notice in the Description page of Project Settings.


#include "AttributeFeature.h"
#include "IFC.h"
#include "Assets.h"
#include "ECS.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/writer.h"

namespace IFC {
	void AttributeFeature::RegisterComponents(flecs::world& world) {
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

	static FString GetValueAsString(const rapidjson::Value& value) {
		FString string;

		if (value.IsString())
			string = UTF8_TO_TCHAR(value.GetString());
		else if (value.IsObject() && value.MemberCount() == 1) // ref
			string = UTF8_TO_TCHAR(value.FindMember("ref")->value.GetString());
		else
			string = JsonValueToString(value);

		string.ReplaceInline(TEXT("$"), TEXT("\\$"));

		return string;
	}

	static FString GetAttributeNestedNameAndValue(const rapidjson::Value& v) {
		if (!v.IsObject())
			return TEXT("");

		FString result;
		for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it) {
			const FString childName = UTF8_TO_TCHAR(it->name.GetString());
			const FString childValue = GetValueAsString(it->value);

			result += TEXT("\n\t\tprefab _ {");
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

	FString GetAttributes(const rapidjson::Value& object) {
		if (!object.HasMember(ATTRIBUTES) || !object[ATTRIBUTES].IsObject())
			return "";

		FString result = "";

		const rapidjson::Value& attributes = object[ATTRIBUTES];
		for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
			const FString nameAndOwner = UTF8_TO_TCHAR(itr->name.GetString());
			FString owner, name;
			nameAndOwner.Split(ATTRIBUTE_SEPARATOR, &owner, &name);

			const rapidjson::Value& attrValue = itr->value;

			result += FString::Printf(TEXT("\tprefab _ : %s {%s%s%s\n\t}\n"),
				*owner,
				*FString::Printf(TEXT("\n\t\t%s"), UTF8_TO_TCHAR(COMPONENT(Attribute))),
				*GetAttributeNameAndValue(name, attrValue),
				*GetAttributeNestedNameAndValue(attrValue));
		}

		return result;
	}
}