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

	FString FormatUUIDs(const FString& input) {
		FString output = input;

		// Match UUIDs in the form xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
		const FRegexPattern uuidPattern(TEXT(R"(([a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}))"));
		FRegexMatcher matcher(uuidPattern, output);

		while (matcher.FindNext()) {
			FString fullMatch = matcher.GetCaptureGroup(0);
			FString cleanedUuid = fullMatch.Replace(TEXT("-"), TEXT(""));
			FString formattedUuid = FString::Printf(TEXT("ID%s"), *cleanedUuid);
			output = output.Replace(*fullMatch, *formattedUuid, ESearchCase::IgnoreCase);
		}

		return output;
	}

	using namespace rapidjson;
	FString BuildHierarchyTree(const rapidjson::Value* node, const TMap<FString, const rapidjson::Value*>& pathToObjectMap, int32 depth) {
		if (!node || !node->HasMember("path") || !(*node)["path"].IsString())
			return TEXT("");

		auto indent = TEXT('\t');
		FString nodeIndent = FString::ChrN(depth, indent);
		FString innerIndent = FString::ChrN(depth + 1, indent);

		FString path = UTF8_TO_TCHAR((*node)["path"].GetString());

		FString output;
		if (depth == 0) // Root node: use path as header			
			output += FString::Printf(TEXT("%s%s {\n"), *nodeIndent, *path);
		else // Child node: name is printed by parent, just open block
			output += FString::Printf(TEXT(": %s {\n"), *path);

		if (node->HasMember("children") && (*node)["children"].IsObject()) {
			const auto& children = (*node)["children"];
			for (auto itr = children.MemberBegin(); itr != children.MemberEnd(); ++itr) {
				if (!itr->value.IsString()) continue;

				FString childName = UTF8_TO_TCHAR(itr->name.GetString());
				FString childPath = UTF8_TO_TCHAR(itr->value.GetString());

				const rapidjson::Value* const* childObjPtr = pathToObjectMap.Find(childPath);
				output += FString::Printf(TEXT("%s%s"), *innerIndent, *childName);

				if (childObjPtr && *childObjPtr)
					output += BuildHierarchyTree(*childObjPtr, pathToObjectMap, depth + 1);
				else // Leaf
					output += FString::Printf(TEXT(": %s {}\n"), *childPath);
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
		TMap<FString, int32> pathCounts;

		// Step 1: Find all objects with "children" and collect child paths
		for (SizeType i = 0; i < dataArray.Size(); ++i) {
			const Value& item = dataArray[i];

			if (item.HasMember("path") && item["path"].IsString()) {
				FString path = UTF8_TO_TCHAR(item["path"].GetString());
				pathCounts.FindOrAdd(path)++;
			}

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

		// Step 4: Map all hierarchy objects
		TMap<FString, const Value*> pathToObjectMap;
		for (const Value* obj : hierarchyObjects) {
			if (obj->HasMember("path") && (*obj)["path"].IsString()) {
				FString path = UTF8_TO_TCHAR((*obj)["path"].GetString());
				pathToObjectMap.Add(path, obj);
			}
		}

		// Step 5: Find the main root (its path appears only once in the entire data)
		const Value* mainRoot = nullptr;
		for (const Value* root : rootObjects) {
			if (root->HasMember("path") && (*root)["path"].IsString()) {
				FString path = UTF8_TO_TCHAR((*root)["path"].GetString());
				if (pathCounts.Contains(path) && pathCounts[path] == 1) {
					mainRoot = root;
					break;
				}
			}
		}

		// Step 6: Output all rootObjects, mainRoot last, prefixed with "prefab " if not mainRoot
		FString result;
		for (const Value* root : rootObjects) {
			if (root == mainRoot) {
				continue; // Skip for now, append last
			}
			result += TEXT("prefab ");
			result += BuildHierarchyTree(root, pathToObjectMap, 0);
		}
		if (mainRoot) {
			result += BuildHierarchyTree(mainRoot, pathToObjectMap, 0);
		}

		return result;
	}

	void LoadIFCFile(flecs::world& world, const FString& path) {
		auto data = Assets::LoadTextFile(path);
		auto formated = FormatUUIDs(data);
		free(data);

		FString hierarchies = GetHierarchies(formated);
		UE_LOG(LogTemp, Log, TEXT("Hierarchies:\n%s"), *hierarchies);
	}
}