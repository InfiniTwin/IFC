// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFC.h"
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
		using namespace ECS;
		world.component<bsi_ifc_presentation_diffuseColor>().member<FLinearColor>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_class>()
			.member<FString>(MEMBER(bsi_ifc_class::Code))
			.member<FString>(MEMBER(bsi_ifc_class::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_spaceBoundary>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_material>()
			.member<FString>(MEMBER(bsi_ifc_material::Code))
			.member<FString>(MEMBER(bsi_ifc_material::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);

		world.component<bsi_ifc_prop_IsExternal>().add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_Volume>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_Height>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_Station>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_TypeName>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_prop_FireRating>().add(flecs::Exclusive).add(flecs::OnInstantiate, flecs::Inherit);

		world.component<bsi_ifc_mat_prop_StrengthClass>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_mat_prop_MoistureContent>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_mat_prop_MassDensity>().member<float>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_mat_prop_GWP>()
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::A1_A3))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::A4))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::A5))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::C2))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::C3))
			.member<float>(MEMBER(bsi_ifc_mat_prop_GWP::D))
			.add(flecs::OnInstantiate, flecs::Inherit);

		world.component<usd_usdgeom_mesh>()
			.member<TArray<float>>(MEMBER(usd_usdgeom_mesh::FaceVertexIndices))
			.member<TArray<FVector3f>>(MEMBER(usd_usdgeom_mesh::Points))
			.add(flecs::OnInstantiate, flecs::Inherit);
		world.component<usd_usdgeom_visibility>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<usd_xformop>().member<TArray<FVector4f>>(MEMBER(usd_xformop::Transform)).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<usd_usdgeom_basiscurves>().member<TArray<FVector3f>>(MEMBER(usd_usdgeom_basiscurves::Points)).add(flecs::OnInstantiate, flecs::Inherit);

		world.component<nlsfb_class>()
			.member<FString>(MEMBER(nlsfb_class::Code))
			.member<FString>(MEMBER(nlsfb_class::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);
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

	bool ExcludeAttribute(const FString& attribute) {
		return HasAttribute(ExcludeAttributes, attribute);
	}

	bool CanIncludeAttributeValues(const FString& attribute) {
		return !HasAttribute(ExcludeAtributesValues, attribute);
	}

	bool IsVectorAttribute(const FString& attribute) {
		return HasAttribute(VectorAttributes, attribute);
	}

	bool IsEnumAttribute(const FString& attribute) {
		return HasAttribute(EnumAttributes, attribute);
	}

	FString FormatName(const FString& fullName) {
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
			if (name.Equals(OPACITY_ATTRIBUTE))
				return FormatAttributeValue(itr->value);
		}
		return TEXT("1");
	}

	FString ProcessAttributes(
		const Value& attributes,
		const TArray<FString>& names,
		const TArray<bool>& includeValues,
		const TArray<bool>& vectors,
		const TArray<bool>& enums) {
		FString result;

		for (int32 i = 0; i < names.Num(); ++i) {
			const FString& attrName = names[i];
			if (ExcludeAttribute(attrName))
				continue;

			FTCHARToUTF8 utf8AttrName(*attrName);
			const char* attrNameUtf8 = utf8AttrName.Get();
			auto memberItr = attributes.FindMember(attrNameUtf8);

			if (memberItr == attributes.MemberEnd())
				continue;

			const Value& attrValue = memberItr->value;
			FString name = FormatName(attrName);

			if (!includeValues[i]) {
				result += FString::Printf(TEXT("\t%s\n"), *name);
				continue;
			}

			// Vector attribute
			if (vectors[i]) {
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

			// Enum attribute
			if (enums[i]) {
				result += FString::Printf(TEXT("\t(%s, %s)\n"), *name, UTF8_TO_TCHAR(attrValue.GetString()));
				continue;
			}

			// Boolean true: print only name
			if (attrValue.IsBool() && attrValue.GetBool() == true) {
				result += FString::Printf(TEXT("\t%s\n"), *name);
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
		TArray<FString> names;
		TArray<bool> vectors;
		TArray<bool> enums;
		TArray<bool> includeValues;

		for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
			FString attrName = UTF8_TO_TCHAR(itr->name.GetString());
			bool isVector = IsVectorAttribute(attrName);
			bool isEnum = IsEnumAttribute(attrName);
			bool includeAttributeValues = CanIncludeAttributeValues(attrName) || isVector || isEnum;

			names.Add(attrName);
			vectors.Add(isVector);
			enums.Add(isEnum);
			includeValues.Add(includeAttributeValues);
		}

		return ProcessAttributes(attributes, names, includeValues, vectors, enums);
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

	FString GetChildren(const Value& obj, bool isPrefab) {
		if (!obj.HasMember(CHILDREN) || !obj[CHILDREN].IsObject())
			return TEXT("");

		const Value& children = obj[CHILDREN];
		FString result;

		for (auto itr = children.MemberBegin(); itr != children.MemberEnd(); ++itr) {
			const FString key = FormatName(UTF8_TO_TCHAR(itr->name.GetString()));
			const Value& val = itr->value;

			if (val.IsString()) {
				const FString valueStr = UTF8_TO_TCHAR(val.GetString());
				result += FString::Printf(TEXT("\t%s%s: %s {}\n"),
					isPrefab ? PREFAB : TEXT(""),
					*key,
					*valueStr);
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
				MergeObjectMembers(existing, obj, INHERITS, allocator);
				MergeObjectMembers(existing, obj, ATTRIBUTES, allocator);
				MergeObjectMembers(existing, obj, CHILDREN, allocator);
			}
		}

		for (const auto& Pair : mergedObjects) {
			Value copy(Pair.Value, allocator);
			mergedArray.PushBack(copy, allocator);
		}

		return mergedArray;
	}

	FString ToFlecsScript(const rapidjson::Value& data, rapidjson::Document::AllocatorType& allocator) {
		rapidjson::Value merged = Merge(data, allocator);
		TArray<const rapidjson::Value*> sorted = Sort(merged);

		TSet<FString> entities; // Find entities: non repeating ID
		for (const rapidjson::Value* obj : sorted) {
			if (!obj || !obj->IsObject()) continue;
			entities.Add(UTF8_TO_TCHAR((*obj)[PATH].GetString()));
		}
		for (const rapidjson::Value* obj : sorted) {
			if (!obj || !obj->IsObject()) continue;
			if ((*obj).HasMember(CHILDREN) && (*obj)[CHILDREN].IsObject()) {
				for (auto& child : (*obj)[CHILDREN].GetObject()) {
					entities.Remove(UTF8_TO_TCHAR(child.value.GetString()));
				}
			}
			if ((*obj).HasMember(INHERITS) && (*obj)[INHERITS].IsObject()) {
				for (auto& inherit : (*obj)[INHERITS].GetObject()) {
					entities.Remove(UTF8_TO_TCHAR(inherit.value.GetString()));
				}
			}
		}

		FString result;
		for (const rapidjson::Value* obj : sorted) {
			if (!obj || !obj->IsObject()) continue;

			FString path = UTF8_TO_TCHAR((*obj)[PATH].GetString());
			bool isPrefab = !entities.Contains(path);

			result += FString::Printf(TEXT("%s%s.%s%s {\n%s%s}\n"),
				isPrefab ? PREFAB : TEXT(""),
				*IFC::Scope(),
				*path,
				*GetInheritances(*obj),
				*GetAttributes(*obj),
				*GetChildren(*obj, isPrefab));
		}
		return result;
	}

	void LoadIFCFiles(flecs::world& world, const TArray<FString>& paths) {
		rapidjson::Document::AllocatorType allocator;
		rapidjson::Document tempDoc;
		allocator = tempDoc.GetAllocator();

		rapidjson::Value combinedData(rapidjson::kArrayType);

		for (const FString& path : paths) {
			auto jsonString = Assets::LoadTextFile(path);
			auto formatted = FormatUUIDs(jsonString);
			free(jsonString);

			rapidjson::Document doc;
			if (doc.Parse(TCHAR_TO_UTF8(*formatted)).HasParseError()) {
				UE_LOG(LogTemp, Error, TEXT(">>> Parse error in file %s: %s"), *path, *FString(GetParseError_En(doc.GetParseError())));
				continue;
			}

			if (!doc.HasMember(DATA) || !doc[DATA].IsArray()) {
				UE_LOG(LogTemp, Warning, TEXT(">>> No valid data array in file: %s"), *path);
				continue;
			}

			for (const auto& entry : doc[DATA].GetArray()) {
				rapidjson::Value copy(entry, allocator);
				combinedData.PushBack(copy, allocator);
			}
		}

		FString code = ToFlecsScript(combinedData, allocator);
		UE_LOG(LogTemp, Log, TEXT(">>> IFC:\n%s"), *code);
		ECS::RunCode(world, paths[0], code);
	}
}