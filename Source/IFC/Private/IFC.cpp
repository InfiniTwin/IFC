// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFC.h"
#include "IFCFeature.h"
#include "Assets.h"
#include "ECS.h"
#include "Containers/Map.h"
#include "rapidjson/document.h"
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

	bool IncludeAttribute(const FString& fullAttribute) {
		for (const TCHAR* attribute : AllowedAttributes) {
			int32 attrLen = fullAttribute.Len();
			int32 allowedLen = FCString::Strlen(attribute);

			if (attrLen >= allowedLen) {
				const TCHAR* fullStr = *fullAttribute;
				const TCHAR* attrSuffix = fullStr + (attrLen - allowedLen);

				if (FCString::Strcmp(attrSuffix, attribute) == 0)
					return true;
			}
		}
		return false;
	}

	FString FormatAttributeName(const FString& fullName) {
		FString trimmed;
		if (fullName.Contains(TEXT("::"))) {
			int32 idx;
			fullName.FindLastChar(':', idx);
			// idx is index of last ':', but double colon, so -1 more for second colon
			trimmed = fullName.RightChop(idx + 1);
		} else {
			trimmed = fullName;
		}

		// Uppercase first letter
		if (trimmed.Len() > 0) {
			TCHAR first = FChar::ToUpper(trimmed[0]);
			trimmed[0] = first;
		}
		return trimmed;
	}
	
	FString FormatAttributeValue(const Value& val) {
		if (val.IsString()) {
			return FString::Printf(TEXT("\"%s\""), *FString(UTF8_TO_TCHAR(val.GetString())));
		} else if (val.IsNumber()) {
			if (val.IsDouble() || val.IsFloat()) {
				return FString::SanitizeFloat(val.GetDouble());
			} else if (val.IsInt64()) {
				return FString::Printf(TEXT("%lld"), val.GetInt64());
			} else {
				return FString::Printf(TEXT("%d"), val.GetInt());
			}
		} else if (val.IsBool()) {
			return val.GetBool() ? TEXT("true") : TEXT("false");
		} else if (val.IsArray()) {
			FString result;
			if (val.Size() > 0 && val[0].IsArray()) {
				result += TEXT("[");
				for (SizeType i = 0; i < val.Size(); ++i) {
					if (i > 0) result += TEXT(", ");
					result += TEXT("{");
					for (SizeType j = 0; j < val[i].Size(); ++j) {
						if (j > 0) result += TEXT(", ");
						result += FormatAttributeValue(val[i][j]);
					}
					result += TEXT("}");
				}
				result += TEXT("]");
			} else {
				result += TEXT("[");
				for (SizeType i = 0; i < val.Size(); ++i) {
					if (i > 0) result += TEXT(", ");
					result += FormatAttributeValue(val[i]);
				}
				result += TEXT("]");
			}
			return result;
		} else if (val.IsObject()) {
			// Optional: You could add support for printing key:value pairs here.
			return TEXT("\"{object}\"");
		} else {
			return TEXT("\"UNKNOWN\"");
		}
	}

	FString ProcessAttributes(const Value& attributes, const TArray<FString>& filteredAttrNames) {
		FString result;

		for (const FString& attrName : filteredAttrNames) {
			FTCHARToUTF8 utf8AttrName(*attrName);
			const char* attrNameUtf8 = utf8AttrName.Get();

			auto memberItr = attributes.FindMember(attrNameUtf8);
			if (memberItr == attributes.MemberEnd())
				continue;

			const Value& attrValue = memberItr->value;
			FString name = FormatAttributeName(attrName);

			if (attrValue.IsBool() && attrValue.GetBool() == true) {
				result += FString::Printf(TEXT("\t%s\n"), *name);
				continue;
			}

			result += FString::Printf(TEXT("\t%s: {"), *name);

			if (attrValue.IsObject()) {
				bool first = true;
				for (auto it = attrValue.MemberBegin(); it != attrValue.MemberEnd(); ++it) {
					if (!first) result += TEXT(", ");
					first = false;
					result += FormatAttributeValue(it->value);
				}
			} else if (attrValue.IsArray()) {
				result += TEXT("[");
				bool firstOuter = true;
				for (SizeType i = 0; i < attrValue.Size(); ++i) {
					const Value& innerVal = attrValue[i];

					if (!firstOuter) result += TEXT(", ");
					firstOuter = false;

					if (innerVal.IsArray()) {
						result += TEXT("{");
						bool firstInner = true;
						for (SizeType j = 0; j < innerVal.Size(); ++j) {
							if (!firstInner) result += TEXT(", ");
							firstInner = false;
							result += FormatAttributeValue(innerVal[j]);
						}
						result += TEXT("}");
					} else {
						result += FormatAttributeValue(innerVal);
					}
				}
				result += TEXT("]");
			} else {
				result += FormatAttributeValue(attrValue);
			}

			result += TEXT("}\n");
		}

		return result;
	}

	FString GetPrefabs(const Value& data) {
		if (!data.IsArray())
			return TEXT("");

		FString result;

		for (SizeType i = 0; i < data.Size(); ++i) {
			const Value& obj = data[i];
			if (!obj.IsObject())
				continue;

			if (!obj.HasMember("attributes") || !obj["attributes"].IsObject())
				continue;

			const Value& attributes = obj["attributes"];

			// Filter attributes: check if any attribute key is in AllowedSchemas
			TArray<FString> filteredAttrNames;
			for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
				FString attrName = UTF8_TO_TCHAR(itr->name.GetString());
				if (IncludeAttribute(attrName)) {
					filteredAttrNames.Add(attrName);
				}
			}

			if (filteredAttrNames.Num() == 0)
				continue;

			FString path = obj.HasMember("path") && obj["path"].IsString()
				? UTF8_TO_TCHAR(obj["path"].GetString())
				: TEXT("");

			result += FString::Printf(TEXT("prefab [IFC].%s {\n"), *path);

			// Process attribute block
			result += ProcessAttributes(attributes, filteredAttrNames);

			result += TEXT("}\n");
		}

		return result;
	}

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

	FString GetHierarchies(const rapidjson::Value& data) {
		TArray<const Value*> hierarchyObjects;
		TArray<const Value*> rootObjects;
		TArray<const Value*> childObjects;
		TSet<FString> allChildPaths;
		TMap<FString, int32> pathCounts;

		// Step 1: Find all objects with "children" and collect child paths
		for (SizeType i = 0; i < data.Size(); ++i) {
			const Value& item = data[i];

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
			result += TEXT("\nprefab [IFC].") + BuildHierarchyTree(root, pathToObjectMap, 0);
		}
		if (mainRoot) {
			result += TEXT("\n[IFC].") + BuildHierarchyTree(mainRoot, pathToObjectMap, 0);
		}

		return result;
	}

	void LoadIFCFile(flecs::world& world, const FString& path) {
		auto jsonString = Assets::LoadTextFile(path);
		auto formated = FormatUUIDs(jsonString);
		free(jsonString);

		using namespace rapidjson;
		Document doc;

		if (doc.Parse(TCHAR_TO_UTF8(*formated)).HasParseError()) {
			UE_LOG(LogTemp, Error, TEXT("Parse error: %s"), *FString(GetParseError_En(doc.GetParseError())));
			return;
		}

		if (!doc.HasMember("data") || !doc["data"].IsArray()) {
			UE_LOG(LogTemp, Error, TEXT("Missing or invalid 'data' array"));
			return;
		}

		const auto& data = doc["data"];

		FString prefabs = GetPrefabs(data);
		UE_LOG(LogTemp, Log, TEXT(">>> Prefabs:\n%s"), *prefabs);

		FString hierarchies = GetHierarchies(data);
		UE_LOG(LogTemp, Log, TEXT(">>> Hierarchies:\n%s"), *hierarchies);

		ECS::RunScript(world, path, prefabs);
	}
}