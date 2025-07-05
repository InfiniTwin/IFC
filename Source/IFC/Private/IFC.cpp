// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFC.h"
#include <IFCFeature.h>
#include <Assets.h>
#include "Containers/Map.h"
#include "rapidjson/error/en.h"

#define LOCTEXT_NAMESPACE "FIFCModule"

void FIFCModule::StartupModule() {
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FIFCModule::ShutdownModule() {
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FIFCModule, IFC)

namespace IFC {
	FString& Scope() {
		static FString scope = TEXT("");
		return scope;
	}

	void Register(flecs::world& world) {
		IFCFeature::RegisterComponents(world);

		LoadIFCFile(world, "A:/InfiniTwinOrg/IFC5-development/examples/Hello Wall/hello-wall.ifcx");
	}

	using namespace rapidjson;
	FString BuildHierarchyTree(const rapidjson::Value* node, const TMap<FString, const rapidjson::Value*>& pathToObjectMap, int32 depth) {
		if (!node || !node->HasMember("path") || !(*node)["path"].IsString())
			return TEXT("");

		auto indent = TEXT('\t');
		FString nodeIndent = FString::ChrN(depth, indent);
		FString innerIndent = FString::ChrN(depth + 1, indent);
		FString leafIndent = FString::ChrN(depth + 2, indent);

		FString path = UTF8_TO_TCHAR((*node)["path"].GetString());

		FString output;
		if (depth == 0) // Root node: use path as header			
			output += FString::Printf(TEXT("%s%s {\n"), *nodeIndent, *path);
		else // Child node: name is printed by parent, just open block			
			output += TEXT("{\n");

		output += FString::Printf(TEXT("%sId: {\"%s\"}\n"), *innerIndent, *path);

		if (node->HasMember("children") && (*node)["children"].IsObject()) {
			const auto& children = (*node)["children"];
			for (auto itr = children.MemberBegin(); itr != children.MemberEnd(); ++itr) {
				if (!itr->value.IsString()) continue;

				FString childName = UTF8_TO_TCHAR(itr->name.GetString());
				FString childPath = UTF8_TO_TCHAR(itr->value.GetString());

				const rapidjson::Value* const* childObjPtr = pathToObjectMap.Find(childPath);
				output += FString::Printf(TEXT("%s%s "), *innerIndent, *childName);

				if (childObjPtr && *childObjPtr)
					output += BuildHierarchyTree(*childObjPtr, pathToObjectMap, depth + 1);
				else { // Leaf
					output += FString::Printf(TEXT("{\n%sId: {\"%s\"}\n"), *leafIndent, *childPath);
					output += FString::Printf(TEXT("%s}\n"), *FString::ChrN(depth + 1, indent));
				}
			}
		}

		output += FString::Printf(TEXT("%s}\n"), *nodeIndent);
		return output;
	}

	FString GetHierarchies(const FString& jsonString) {
		using namespace rapidjson;
		Document doc;

		if (doc.Parse(TCHAR_TO_UTF8(*jsonString)).HasParseError()) {
			UE_LOG(LogTemp, Error, TEXT(">>> Parse error: %s"), *FString(GetParseError_En(doc.GetParseError())));
			return TEXT("");
		}

		if (!doc.HasMember("data") || !doc["data"].IsArray()) {
			UE_LOG(LogTemp, Error, TEXT(">>> Missing or invalid 'data' array"));
			return TEXT("");
		}

		const auto& dataArray = doc["data"];

		TArray<const Value*> hierarchyObjects;
		TArray<const Value*> rootObjects;
		TArray<const Value*> childObjects;
		TSet<FString> allChildPaths;

		// Step 1: Find all objects with "children" and collect child paths
		for (SizeType i = 0; i < dataArray.Size(); ++i) {
			const Value& item = dataArray[i];

			if (item.HasMember("children") && item["children"].IsObject()) {
				hierarchyObjects.Add(&item);

				const Value& children = item["children"];
				for (auto itr = children.MemberBegin(); itr != children.MemberEnd(); ++itr) {
					if (itr->value.IsString()) {
						FString childPath = UTF8_TO_TCHAR(itr->value.GetString());
						allChildPaths.Add(childPath);
					}
				}
			}
		}

		// Step 2: Filter root objects (whose path doesn't appear in any child map)
		for (const Value* item : hierarchyObjects) {
			if (item->HasMember("path") && (*item)["path"].IsString()) {
				FString path = UTF8_TO_TCHAR((*item)["path"].GetString());

				if (!allChildPaths.Contains(path)) {
					rootObjects.Add(item);
				}
			}
		}

		// Step 3: Child objects = hierarchyObjects - rootObjects
		for (const Value* item : hierarchyObjects) {
			if (!rootObjects.Contains(item)) {
				childObjects.Add(item);
			}
		}

		// Now create the map from all hierarchyObjects (root + child)
		TMap<FString, const rapidjson::Value*> pathToObjectMap;
		for (const rapidjson::Value* obj : hierarchyObjects) {
			if (obj->HasMember("path") && (*obj)["path"].IsString()) {
				FString path = UTF8_TO_TCHAR((*obj)["path"].GetString());
				pathToObjectMap.Add(path, obj);
			}
		}

		// Now use this to build the hierarchy recursively
		FString result;
		for (const Value* root : rootObjects) {
			result += BuildHierarchyTree(root, pathToObjectMap, 0);
		}

		return result;
	}

	void LoadIFCFile(flecs::world& world, const FString& path) {
		auto data = Assets::LoadTextFile(path);

		FString hierarchies = GetHierarchies(data);
		UE_LOG(LogTemp, Log, TEXT("Hierarchies:\n%s"), *hierarchies);

		free(data);
	}
}