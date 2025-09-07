// Copyright Epic Games, Inc. All Rights Reserved.

#include "LayerFeature.h"
#include "IFC.h"
#include "Assets.h"
#include "ECS.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/writer.h"

namespace IFC {
	void LayerFeature::CreateComponents(flecs::world& world) {
		using namespace ECS;
		world.component<Layer>();
		world.component<Path>().member<FString>(VALUE);
		world.component<Id>().member<FString>(VALUE);
		world.component<IfcxVersion>().member<FString>(VALUE);
		world.component<DataVersion>().member<FString>(VALUE);
		world.component<Author>().member<FString>(VALUE);
		world.component<Timestamp>().member<FString>(VALUE);

		world.component<Owner>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
	}

	void LayerFeature::CreateQueries(flecs::world& world) {
		world.component<QueryLayers>();
		world.set(QueryLayers{
			world.query_builder<>(COMPONENT(QueryLayers))
			.with<Layer>()
			.with<Id>()
			.cached().build() });
	};

	using namespace rapidjson;

	FString GetOwnerPath(const FString& layerPath) {
		int32 lastDotIndex = INDEX_NONE;
		layerPath.FindLastChar('.', lastDotIndex);
		FString prefix = layerPath.Left(lastDotIndex + 1);
		FString suffix = layerPath.Mid(lastDotIndex + 1);
		return prefix + UTF8_TO_TCHAR(OWNER) + suffix;
	}

	FString ParseLayer(const rapidjson::Value& header, const FString path, const TArray<FString>& components) {
		FString layer = IFC::Scope() + "." + IFC::FormatUUID(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));

		FString result = FString::Printf(TEXT("%s {\n"), *layer);

		for (const FString& component : components)
			result += FString::Printf(TEXT("\t%s\n"), *component);

		result += FString::Printf(TEXT("\t%s: {\"%s\"}\n"), UTF8_TO_TCHAR(COMPONENT(Path)), *path);

		for (auto it = header.MemberBegin(); it != header.MemberEnd(); ++it) {
			FString componentName = UTF8_TO_TCHAR(it->name.GetString());
			componentName[0] = FChar::ToUpper(componentName[0]);
			FString component = IFC::FormatName(componentName);
			FString value = FString::Printf(TEXT("\"%s\""), *FString(UTF8_TO_TCHAR(it->value.GetString())));
			result += FString::Printf(TEXT("\t%s: {%s}\n"), *component, *value);
		}
		result += TEXT("}\n");

		result += FString::Printf(TEXT("prefab %s {\n\t%s: {\"%s\"}\n}\n"),
			*GetOwnerPath(layer),
			UTF8_TO_TCHAR(OWNER),
			*layer);

		return result;
	}

	void AddLayers(flecs::world& world, const TArray<FString>& paths, const TArray<FString>& components) {
		if (paths.Num() < 1) return;

		rapidjson::Document tempDoc;
		rapidjson::Document::AllocatorType& allocator = tempDoc.GetAllocator();
		rapidjson::Value combinedData(rapidjson::kArrayType);
		FString code;

		for (const FString& path : paths) {
			bool exists = false;
			world.try_get<QueryLayers>()->Value.each([&path, &exists](flecs::entity layer) {
				if (layer.try_get<Path>()->Value == path) {
					exists = true;
					return;
				}
				});

			if (exists)
				continue;

			auto jsonString = Assets::LoadTextFile(path);
			auto formatted = FormatUUID(jsonString);
			free(jsonString);

			rapidjson::Document doc;
			if (doc.Parse(TCHAR_TO_UTF8(*formatted)).HasParseError()) {
				UE_LOG(LogTemp, Error, TEXT(">>> Parse error in file %s: %s"), *path, *FString(GetParseError_En(doc.GetParseError())));
				continue;
			}

			if (!doc.HasMember(HEADER) || !doc[HEADER].IsObject()) {
				UE_LOG(LogTemp, Warning, TEXT(">>> Invalid Header: %s"), *path);
				continue;
			}

			if (!doc.HasMember(DATA_KEY) || !doc[DATA_KEY].IsArray()) {
				UE_LOG(LogTemp, Warning, TEXT(">>> Invalid Data: %s"), *path);
				continue;
			}

			code += ParseLayer(doc[HEADER], path, components);
		}

		ECS::RunCode(world, paths[0], code);
	}
}