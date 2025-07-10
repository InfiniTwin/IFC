// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFC.h"
#include "IFCFeature.h"
#include "Assets.h"
#include "ECS.h"
#include "Containers/Map.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#define LOCTEXT_NAMESPACE "FIFCModule"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"


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

#pragma region Hierarchies

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

#pragma endregion

#pragma region Attributes

	bool HasAttribute(const TSet<FString>& attributes, const FString& name) {
		for (const FString& attribute : attributes) {
			if (name == attribute)
				return true;
		}
		return false;
	}

	bool IncludeAttribute(const FString& attribute) {
		return HasAttribute(AllowedAttributes, attribute);
	}

	bool SkipProcessingAttribute(const FString& attribute) {
		return HasAttribute(SkipProcessingAttributes, attribute);
	}

	bool IsVectorAttribute(const FString& attribute) {
		return HasAttribute(VectorAttributes, attribute);
	}

	FString FormatAttributeName(const FString& fullName) {
		FString formatted = fullName;
		for (const FString& symbol : { TEXT("::"), TEXT("-") }) {
			formatted = formatted.Replace(*symbol, TEXT("_"));
		}
		return formatted;
	}

	FString FormatAttributeValue(const Value& val, bool isInnerArray = false) {
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

			if (isInnerArray) {
				// Inner array: use double curly braces
				result += TEXT("{{");
				for (SizeType i = 0; i < val.Size(); ++i) {
					if (i > 0) result += TEXT(", ");
					result += FormatAttributeValue(val[i]);
				}
				result += TEXT("}}");
			} else {
				// Outer array: use square brackets
				result += TEXT("[");
				for (SizeType i = 0; i < val.Size(); ++i) {
					if (i > 0) result += TEXT(", ");
					// If this element is an array, format it as inner array
					if (val[i].IsArray()) {
						result += FormatAttributeValue(val[i], true);
					} else {
						result += FormatAttributeValue(val[i]);
					}
				}
				result += TEXT("]");
			}
			return result;
		} else if (val.IsObject()) {
			return TEXT("\"{object}\"");
		} else {
			return TEXT("\"UNKNOWN\"");
		}
	}

	FString GetOpacity(const Value& attributes) {
		for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
			FString name = UTF8_TO_TCHAR(itr->name.GetString());
			if (name.Equals(OPACITY_ATTRIBUTE)) {
				return FormatAttributeValue(itr->value);
			}
		}
		return TEXT("1");
	}

	FString ProcessAttributes(const Value& attributes, const TArray<FString>& filteredAttrNames, TArray<bool> vectors) {
		FString result;

		for (int32 i = 0; i < filteredAttrNames.Num(); ++i) {
			const FString& attrName = filteredAttrNames[i];
			if (SkipProcessingAttribute(attrName))
				continue;
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

			if (attrValue.IsArray() && vectors[i]) {
				// Vector attribute: format as {{ ... }}
				result += FString::Printf(TEXT("\t%s: {{"), *name);
				for (SizeType j = 0; j < attrValue.Size(); ++j) {
					if (j > 0) result += TEXT(", ");
					result += FormatAttributeValue(attrValue[j]);
				}

				if (name.Equals(DIFFUSECOLOR_COMPONENT)) {
					result += TEXT(", ") + GetOpacity(attributes);
				}

				result += TEXT("}}\n");
				continue;
			}

			// Default formatting
			result += FString::Printf(TEXT("\t%s: {"), *name);

			if (attrValue.IsObject()) {
				bool first = true;
				for (auto it = attrValue.MemberBegin(); it != attrValue.MemberEnd(); ++it) {
					if (!first) result += TEXT(", ");
					first = false;
					result += FormatAttributeValue(it->value);
				}
			} else if (attrValue.IsArray()) {
				result += FormatAttributeValue(attrValue);
			} else {
				result += FormatAttributeValue(attrValue);
			}

			result += TEXT("}\n");
		}

		return result;
	}

	FString GetAttributes(const Value& obj) {
		if (!obj.HasMember("attributes") || !obj["attributes"].IsObject())
			return TEXT("");

		const Value& attributes = obj["attributes"];
		TArray<FString> filteredAttrNames;
		TArray<bool> vectors;

		for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
			FString attrName = UTF8_TO_TCHAR(itr->name.GetString());
			bool isVector = IsVectorAttribute(attrName);
			if (IncludeAttribute(attrName) || isVector) {
				filteredAttrNames.Add(attrName);
				vectors.Add(isVector);
			}
		}

		if (filteredAttrNames.Num() == 0)
			return TEXT("");

		return ProcessAttributes(attributes, filteredAttrNames, vectors);
	}

#pragma endregion

	FString GetInheritances(const Value& obj) {
		if (!obj.HasMember("inherits") || !obj["inherits"].IsObject())
			return TEXT("");

		const Value& inherits = obj["inherits"];
		TArray<FString> inheritIDs;

		for (auto itr = inherits.MemberBegin(); itr != inherits.MemberEnd(); ++itr) {
			const Value& val = itr->value;
			if (val.IsString()) {
				inheritIDs.Add(UTF8_TO_TCHAR(val.GetString()));
			}
		}

		if (inheritIDs.Num() == 0)
			return TEXT("");

		return TEXT(": ") + FString::Join(inheritIDs, TEXT(", "));
	}

	FString GetChildren(const Value& obj) {
		if (!obj.HasMember("children") || !obj["children"].IsObject())
			return TEXT("");

		const Value& children = obj["children"];
		FString result;

		for (auto itr = children.MemberBegin(); itr != children.MemberEnd(); ++itr) {
			const FString key = UTF8_TO_TCHAR(itr->name.GetString());
			const Value& val = itr->value;

			if (val.IsString()) {
				const FString valueStr = UTF8_TO_TCHAR(val.GetString());
				result += FString::Printf(TEXT("    %s: %s {}\n"), *key, *valueStr);
			}
		}

		return result;
	}

	TArray<const Value*> ReorderChildrenLast(const TArray<const Value*>& sorted) {
		TArray<const Value*> noChildren;
		TArray<const Value*> withChildren;

		for (const Value* obj : sorted) {
			if (obj->HasMember("children") && (*obj)["children"].IsObject()) {
				withChildren.Add(obj);
			} else {
				noChildren.Add(obj);
			}
		}

		// Children go last
		noChildren.Append(withChildren);
		return noChildren;
	}

	TArray<const rapidjson::Value*> TopologicallySortObjects(const rapidjson::Value& data) {
		using namespace rapidjson;

		if (!data.IsArray())
			return {};

		// Step 1: Build objectMap and idToPath
		TMap<FString, const Value*> objectMap;
		TMap<FString, FString> idToPath;
		TArray<const Value*> originalOrder; // Keep original order for fallback
		TSet<const Value*> sortedSet;

		for (SizeType i = 0; i < data.Size(); ++i) {
			const Value& obj = data[i];
			originalOrder.Add(&obj);

			if (obj.HasMember("path") && obj["path"].IsString()) {
				FString path = UTF8_TO_TCHAR(obj["path"].GetString());
				objectMap.Add(path, &obj);

				// Extract ID portion (e.g. "ID2550...") from path for lookup
				int32 idIndex = path.Find(TEXT("ID"));
				if (idIndex != INDEX_NONE) {
					FString id = path.Mid(idIndex); // includes "ID..."
					idToPath.Add(id, path);
				}
			}
		}

		// Step 2: Build dependency graph
		TMap<FString, TArray<FString>> dependencyGraph;

		auto ResolveReference = [&](const FString& ref) -> FString {
			const FString* found = idToPath.Find(ref);
			return found ? *found : ref;
		};

		for (const auto& Pair : objectMap) {
			FString path = Pair.Key;
			const Value* obj = Pair.Value;

			TArray<FString> dependencies;

			// Children
			if (obj->HasMember("children") && (*obj)["children"].IsObject()) {
				const Value& children = (*obj)["children"];
				for (auto itr = children.MemberBegin(); itr != children.MemberEnd(); ++itr) {
					if (itr->value.IsString()) {
						FString ref = UTF8_TO_TCHAR(itr->value.GetString());
						FString resolved = ResolveReference(ref);
						dependencies.Add(resolved);
					}
				}
			}

			// Inherits
			if (obj->HasMember("inherits")) {
				const Value& inherits = (*obj)["inherits"];
				if (inherits.IsObject()) {
					for (auto itr = inherits.MemberBegin(); itr != inherits.MemberEnd(); ++itr) {
						if (itr->value.IsString()) {
							FString ref = UTF8_TO_TCHAR(itr->value.GetString());
							FString resolved = ResolveReference(ref);
							dependencies.Add(resolved);
						}
					}
				} else if (inherits.IsArray()) {
					for (SizeType i = 0; i < inherits.Size(); ++i) {
						if (inherits[i].IsString()) {
							FString ref = UTF8_TO_TCHAR(inherits[i].GetString());
							FString resolved = ResolveReference(ref);
							dependencies.Add(resolved);
						}
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("Object %s depends on:"), *path);
			for (const FString& dep : dependencies) {
				UE_LOG(LogTemp, Log, TEXT("   -> %s"), *dep);
			}

			dependencyGraph.Add(path, dependencies);
		}

		// Step 3: Topological sort (DFS)
		TSet<FString> visited;
		TSet<FString> tempMarks;
		TArray<FString> sortedPaths;

		TFunction<void(const FString&)> Visit = [&](const FString& node) {
			if (visited.Contains(node))
				return;
			if (tempMarks.Contains(node)) {
				UE_LOG(LogTemp, Warning, TEXT("Cyclic dependency detected at %s"), *node);
				return;
			}

			tempMarks.Add(node);
			for (const FString& dep : dependencyGraph.FindRef(node)) {
				if (objectMap.Contains(dep))
					Visit(dep);
			}
			tempMarks.Remove(node);
			visited.Add(node);
			sortedPaths.Add(node);
		};

		for (const auto& Pair : objectMap) {
			Visit(Pair.Key);
		}

		// Step 4: Construct result
		TArray<const Value*> result;

		for (const FString& path : sortedPaths) {
			const Value* obj = objectMap.FindRef(path);
			if (obj) {
				result.Add(obj);
				sortedSet.Add(obj);
			}
		}

		// Add remaining objects that were not sorted (orphans)
		for (const Value* obj : originalOrder) {
			if (!sortedSet.Contains(obj)) {
				result.Add(obj);
			}
		}

		// Optional: move parents before children (if needed for output)
		return ReorderChildrenLast(result);
	}

	Value MergePrefabs(const Value& inputArray, Document::AllocatorType& allocator) {
		Value mergedArray(kArrayType);
		TMap<FString, Value> mergedObjects;

		for (SizeType i = 0; i < inputArray.Size(); ++i) {
			const Value& obj = inputArray[i];
			if (!obj.IsObject() || !obj.HasMember("path") || !obj["path"].IsString())
				continue;

			FString pathStr = UTF8_TO_TCHAR(obj["path"].GetString());

			// If this path hasn't been seen, just copy it into the map
			if (!mergedObjects.Contains(pathStr)) {
				Value newObj(kObjectType);
				newObj.CopyFrom(obj, allocator);
				mergedObjects.Add(pathStr, MoveTemp(newObj));
			} else {
				Value& existing = mergedObjects[pathStr];

				// Merge "attributes"
				if (obj.HasMember("attributes") && obj["attributes"].IsObject()) {
					Value& existingAttrs = existing.HasMember("attributes")
						? existing["attributes"]
						: existing.AddMember("attributes", Value(kObjectType), allocator)["attributes"];

					const Value& newAttrs = obj["attributes"];
					for (auto it = newAttrs.MemberBegin(); it != newAttrs.MemberEnd(); ++it) {
						// Overwrite or insert attribute
						Value key(it->name, allocator);
						Value val(it->value, allocator);
						existingAttrs.RemoveMember(key); // avoid duplicate keys
						existingAttrs.AddMember(key, val, allocator);
					}
				}

				// Merge "inherits"
				if (obj.HasMember("inherits") && obj["inherits"].IsArray()) {
					Value& existingInherits = existing.HasMember("inherits")
						? existing["inherits"]
						: existing.AddMember("inherits", Value(kArrayType), allocator)["inherits"];

					const Value& newInherits = obj["inherits"];
					for (SizeType j = 0; j < newInherits.Size(); ++j) {
						const Value& inheritVal = newInherits[j];

						// Prevent duplicates
						bool alreadyExists = false;
						for (SizeType k = 0; k < existingInherits.Size(); ++k) {
							if (existingInherits[k] == inheritVal) {
								alreadyExists = true;
								break;
							}
						}
						if (!alreadyExists) {
							Value copyVal(inheritVal, allocator);
							existingInherits.PushBack(copyVal, allocator);
						}
					}
				}
			}
		}

		for (const auto& Pair : mergedObjects) {
			Value copy(Pair.Value, allocator);
			mergedArray.PushBack(copy, allocator);
		}

		return mergedArray;
	}

	FString GetPrefabs(rapidjson::Document& doc) {
		if (!doc.HasMember("data") || !doc["data"].IsArray()) {
			return TEXT("");
		}

		auto& data = doc["data"];
		auto& allocator = doc.GetAllocator();

		rapidjson::Value merged = MergePrefabs(data, allocator);

		// Convert merged (rapidjson::Value) into a stringified JSON (UTF-8)
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		merged.Accept(writer);

		// Convert to FString and log
		FString mergedJsonString = UTF8_TO_TCHAR(buffer.GetString());
		UE_LOG(LogTemp, Log, TEXT("Merged Prefabs JSON:\n%s"), *mergedJsonString);

		TArray<const rapidjson::Value*> sortedObjects = TopologicallySortObjects(merged);
		FString result;

		for (const rapidjson::Value* obj : sortedObjects) {
			if (!obj || !obj->IsObject())
				continue;

			FString path = obj->HasMember("path") && (*obj)["path"].IsString()
				? UTF8_TO_TCHAR((*obj)["path"].GetString())
				: TEXT("");

			FString inheritsBlock = GetInheritances(*obj);
			FString attributesBlock = GetAttributes(*obj);
			FString childrenBlock = GetChildren(*obj);

			if (inheritsBlock.IsEmpty() && attributesBlock.IsEmpty() && childrenBlock.IsEmpty())
				continue;

			result += FString::Printf(TEXT("prefab [IFC].%s%s {\n"), *path, *inheritsBlock);
			result += attributesBlock;
			result += childrenBlock;
			result += TEXT("}\n");
		}

		return result;
	}

	void LoadIFCFile(flecs::world& world, const FString& path) {
		auto jsonString = Assets::LoadTextFile(path);
		auto formated = FormatUUIDs(jsonString);
		free(jsonString);

		Document doc;

		if (doc.Parse(TCHAR_TO_UTF8(*formated)).HasParseError()) {
			UE_LOG(LogTemp, Error, TEXT("Parse error: %s"), *FString(GetParseError_En(doc.GetParseError())));
			return;
		}

		FString prefabs = GetPrefabs(doc);
		UE_LOG(LogTemp, Log, TEXT(">>> Prefabs:\n%s"), *prefabs);

		ECS::RunScript(world, path, prefabs);
	}
}