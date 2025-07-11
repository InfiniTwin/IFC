// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFC.h"
#include "IFCFeature.h"
#include "Assets.h"
#include "ECS.h"
#include "Containers/Map.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "Algo/TopologicalSort.h"

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

#pragma region Attributes

	bool HasAttribute(const TSet<FString>& attributes, const FString& name) {
		for (const FString& attribute : attributes) {
			if (name == attribute)
				return true;
		}
		return false;
	}

	bool CanIncludeAttributeValues(const FString& attribute) {
		return !HasAttribute(ExcludeAtributesValues, attribute);
	}

	bool ExcludeAttribute(const FString& attribute) {
		return HasAttribute(ExcludeAttributes, attribute);
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
		}
		else if (val.IsNumber()) {
			if (val.IsDouble() || val.IsFloat()) {
				return FString::SanitizeFloat(val.GetDouble());
			}
			else if (val.IsInt64()) {
				return FString::Printf(TEXT("%lld"), val.GetInt64());
			}
			else {
				return FString::Printf(TEXT("%d"), val.GetInt());
			}
		}
		else if (val.IsBool()) {
			return val.GetBool() ? TEXT("true") : TEXT("false");
		}
		else if (val.IsArray()) {
			FString result;

			if (isInnerArray) {
				// Inner array: use double curly braces
				result += TEXT("{{");
				for (SizeType i = 0; i < val.Size(); ++i) {
					if (i > 0) result += TEXT(", ");
					result += FormatAttributeValue(val[i]);
				}
				result += TEXT("}}");
			}
			else {
				// Outer array: use square brackets
				result += TEXT("[");
				for (SizeType i = 0; i < val.Size(); ++i) {
					if (i > 0) result += TEXT(", ");
					// If this element is an array, format it as inner array
					if (val[i].IsArray()) {
						result += FormatAttributeValue(val[i], true);
					}
					else {
						result += FormatAttributeValue(val[i]);
					}
				}
				result += TEXT("]");
			}
			return result;
		}
		else if (val.IsObject()) {
			return TEXT("\"{object}\"");
		}
		else {
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

	FString ProcessAttributes(const Value& attributes, const TArray<FString>& attrNames, const TArray<bool>& isVectors, const TArray<bool>& includeAttributesValues) {
		FString result;

		for (int32 i = 0; i < attrNames.Num(); ++i) {
			const FString& attrName = attrNames[i];
			if (ExcludeAttribute(attrName))
				continue;

			FTCHARToUTF8 utf8AttrName(*attrName);
			const char* attrNameUtf8 = utf8AttrName.Get();
			auto memberItr = attributes.FindMember(attrNameUtf8);

			if (memberItr == attributes.MemberEnd())
				continue;

			const Value& attrValue = memberItr->value;
			FString name = FormatAttributeName(attrName);

			if (!includeAttributesValues[i]) {
				result += FString::Printf(TEXT("\t%s\n"), *name);
				continue;
			}

			// Boolean true: print only name
			if (attrValue.IsBool() && attrValue.GetBool() == true) {
				result += FString::Printf(TEXT("\t%s\n"), *name);
				continue;
			}

			// Vector attribute
			if (attrValue.IsArray() && isVectors[i]) {
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
			}
			else {
				result += FormatAttributeValue(attrValue);
			}

			result += TEXT("}\n");
		}

		return result;
	}

	FString GetAttributes(const Value& obj) {
		if (!obj.HasMember(ATTRIBUTES) || !obj[ATTRIBUTES].IsObject())
			return TEXT("");

		const Value& attributes = obj[ATTRIBUTES];
		TArray<FString> attrNames;
		TArray<bool> isVectorFlags;
		TArray<bool> includeAttributesValues;

		for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
			FString attrName = UTF8_TO_TCHAR(itr->name.GetString());
			bool isVector = IsVectorAttribute(attrName);
			bool includeAttributeValues = CanIncludeAttributeValues(attrName) || isVector;

			attrNames.Add(attrName);
			isVectorFlags.Add(isVector);
			includeAttributesValues.Add(includeAttributeValues);
		}

		return ProcessAttributes(attributes, attrNames, isVectorFlags, includeAttributesValues);
	}

#pragma endregion

	FString GetInheritances(const Value& obj) {
		if (!obj.HasMember(INHERITS) || !obj[INHERITS].IsObject())
			return TEXT("");

		const Value& inherits = obj[INHERITS];
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
		if (!obj.HasMember(CHILDREN) || !obj[CHILDREN].IsObject())
			return TEXT("");

		const Value& children = obj[CHILDREN];
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

	TArray<const rapidjson::Value*> Sort(const rapidjson::Value& dataArray) {
		TMap<FString, const rapidjson::Value*> objectMap;
		TMap<FString, TArray<FString>> dependencies;

		// Step 1: Build object map and empty dependency list
		for (auto& entry : dataArray.GetArray()) {
			if (!entry.HasMember(PATH) || !entry[PATH].IsString()) {
				continue;
			}
			FString id = UTF8_TO_TCHAR(entry[PATH].GetString());
			objectMap.Add(id, &entry);
			dependencies.Add(id, {});
		}

		// Step 2: Fill in dependencies
		for (auto& entry : dataArray.GetArray()) {
			if (!entry.HasMember(PATH) || !entry[PATH].IsString()) {
				continue;
			}

			FString id = UTF8_TO_TCHAR(entry[PATH].GetString());

			if (entry.HasMember(CHILDREN) && entry[CHILDREN].IsObject()) {
				for (auto& child : entry[CHILDREN].GetObject()) {
					FString childId = UTF8_TO_TCHAR(child.value.GetString());
					if (dependencies.Contains(id)) {
						dependencies[id].Add(childId); // id depends on child
					}
				}
			}

			if (entry.HasMember(INHERITS) && entry[INHERITS].IsObject()) {
				for (auto& inherit : entry[INHERITS].GetObject()) {
					FString baseId = UTF8_TO_TCHAR(inherit.value.GetString());
					if (dependencies.Contains(id)) {
						dependencies[id].Add(baseId); // id depends on base
					}
				}
			}
		}

		// Step 3: List of all UUIDs to sort
		TArray<FString> sortedIds;
		dependencies.GetKeys(sortedIds);

		// Step 4: Sort them using correct lambda: return array of dependencies for given node
		bool success = Algo::TopologicalSort(sortedIds, [&dependencies](const FString& id) -> const TArray<FString>&{
			return dependencies[id]; // id depends on these
			});

		if (!success) {
			UE_LOG(LogTemp, Warning, TEXT("Cyclic dependency detected in prefab graph."));
		}

		// Step 5: Convert back to JSON pointers
		TArray<const rapidjson::Value*> sortedObjects;
		for (const FString& id : sortedIds) {
			if (const rapidjson::Value** value = objectMap.Find(id)) {
				sortedObjects.Add(*value);
			}
		}

		return sortedObjects;
	}
	
	void MergeObjectMembers(Value& target, const Value& source, const char* memberName, Document::AllocatorType& allocator) {
		if (!source.HasMember(memberName) || !source[memberName].IsObject())
			return;

		if (!target.HasMember(memberName)) {
			target.AddMember(Value(memberName, allocator), Value(kObjectType), allocator);
		}

		Value& targetObject = target[memberName];
		const Value& sourceObject = source[memberName];

		for (auto itr = sourceObject.MemberBegin(); itr != sourceObject.MemberEnd(); ++itr) {
			Value key(itr->name, allocator);
			Value val(itr->value, allocator);
			targetObject.RemoveMember(key);
			targetObject.AddMember(key, val, allocator);
		}
	}

	Value Merge(const Value& inputArray, Document::AllocatorType& allocator) {
		Value mergedArray(kArrayType);
		TMap<FString, Value> mergedObjects;

		for (SizeType i = 0; i < inputArray.Size(); ++i) {
			const Value& obj = inputArray[i];
			if (!obj.IsObject() || !obj.HasMember(PATH) || !obj[PATH].IsString())
				continue;

			FString pathStr = UTF8_TO_TCHAR(obj[PATH].GetString());

			if (!mergedObjects.Contains(pathStr)) {
				Value newObj(kObjectType);
				newObj.CopyFrom(obj, allocator);
				mergedObjects.Add(pathStr, MoveTemp(newObj));
			}
			else {
				Value& existing = mergedObjects[pathStr];
				MergeObjectMembers(existing, obj, ATTRIBUTES, allocator);
				MergeObjectMembers(existing, obj, INHERITS, allocator);
			}
		}

		for (const auto& Pair : mergedObjects) {
			Value copy(Pair.Value, allocator);
			mergedArray.PushBack(copy, allocator);
		}

		return mergedArray;
	}

	FString GetPrefabs(rapidjson::Document& doc) {
		if (!doc.HasMember(DATA) || !doc[DATA].IsArray()) {
			return TEXT("");
		}

		auto& data = doc[DATA];
		auto& allocator = doc.GetAllocator();

		rapidjson::Value merged = Merge(data, allocator);
		TArray<const rapidjson::Value*> sorted = Sort(merged);
		FString result;

		TSet<FString> entities;

		// Step 1: Add all paths as candidates
		for (const rapidjson::Value* obj : sorted) {
			if (!obj || !obj->IsObject()) continue;
			entities.Add(UTF8_TO_TCHAR((*obj)[PATH].GetString()));
		}

		// Step 2: Remove any path that is referenced by others
		for (const rapidjson::Value* obj : sorted) {
			if (!obj || !obj->IsObject()) continue;

			// Remove children
			if ((*obj).HasMember(CHILDREN) && (*obj)[CHILDREN].IsObject()) {
				for (auto& child : (*obj)[CHILDREN].GetObject()) {
					entities.Remove(UTF8_TO_TCHAR(child.value.GetString()));
				}
			}

			// Remove inherits
			if ((*obj).HasMember(INHERITS) && (*obj)[INHERITS].IsObject()) {
				for (auto& inherit : (*obj)[INHERITS].GetObject()) {
					entities.Remove(UTF8_TO_TCHAR(inherit.value.GetString()));
				}
			}
		}

		for (const rapidjson::Value* obj : sorted) {
			if (!obj || !obj->IsObject()) continue;

			FString path = UTF8_TO_TCHAR((*obj)[PATH].GetString());
			bool isPrefab = !entities.Contains(path);

			result += FString::Printf(TEXT("%s%s.%s%s {\n%s%s}\n"),
				isPrefab ? TEXT("prefab ") : TEXT(""),
				*IFC::Scope(),
				*path,
				*GetInheritances(*obj),
				*GetAttributes(*obj),
				*GetChildren(*obj));
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