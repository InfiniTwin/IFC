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
		world.component<Attribute>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
	}

    FString GetAttributes(const rapidjson::Value& object) {
        if (!object.HasMember(ATTRIBUTES) || !object[ATTRIBUTES].IsObject())
            return "";

        FString result = "";

        const Value& attributes = object[ATTRIBUTES];
        for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
            const FString nameAndOwner = UTF8_TO_TCHAR(itr->name.GetString());
            FString owner, name;
            nameAndOwner.Split(ATTRIBUTE_SEPARATOR, &owner, &name);
;
            auto nameComponent = FString::Printf(TEXT("\t\t%s: {\"%s\"}"), UTF8_TO_TCHAR(COMPONENT(Name)), *name);
            auto attributeComponent = FString::Printf(TEXT("\t\t%s"), UTF8_TO_TCHAR(COMPONENT(Attribute)));

            result += FString::Printf(TEXT("\t_ : %s {\n%s\n%s\n\t}\n"),
                *owner,
                *nameComponent,
                *attributeComponent);

            //result += FString::Printf(TEXT("\t{\n%s\n\t}\n"),
            //    *IFC::FormatName(name),
            //    *nameComponent);
        }

        return result;
    }
}