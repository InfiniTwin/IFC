// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFC.h"
#include "LayerFeature.h"
#include "AttributeFeature.h"
#include "Assets.h"
#include "ECS.h"
#include "ECSCore.h"
#include "Containers/Map.h"
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
		LayerFeature::RegisterComponents(world);
		AttributeFeature::RegisterComponents(world);

		using namespace ECS;

		world.component<Name>().member<FString>(VALUE);
		world.component<IFCData>();
		world.component<QueryIFCData>();
		world.set(QueryIFCData{
			world.query_builder<IFCData>(COMPONENT(QueryIFCData))
			.with(flecs::Prefab).optional()
			.cached().build() });

		world.component<Hierarchy>();

		world.component<bsi_ifc_presentation_diffuseColor>().member<FLinearColor>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_class>()
			.member<FString>(MEMBER(bsi_ifc_class::Code))
			.member<FString>(MEMBER(bsi_ifc_class::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_spaceBoundary_relatedelement>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_spaceBoundary_relatingspace>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<bsi_ifc_material>()
			.member<FString>(MEMBER(bsi_ifc_material::Code))
			.member<FString>(MEMBER(bsi_ifc_material::Uri))
			.add(flecs::OnInstantiate, flecs::Inherit);

		world.component<bsi_ifc_prop_IsExternal>().member<bool>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
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

	FString FormatUUID(const FString& input) {
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

	FString FormatName(const FString& name) {
		FString formatted = name;
		for (const FString& symbol : { TEXT("::"), TEXT("-") })
			formatted = formatted.Replace(*symbol, TEXT("_"));
		return formatted;
	}

	FString CleanName(const FString& name) {
		return name.Replace(TEXT("_"), TEXT(" "));
	}

	using namespace rapidjson;

	FString JsonValueToString(const rapidjson::Value& val) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		val.Accept(writer);
		return UTF8_TO_TCHAR(buffer.GetString());
	}

#pragma region Attributes

	bool HasAttribute(const TSet<FString>& attributes, const FString& name) {
		for (const FString& attribute : attributes)
			if (name.Contains(attribute))
				return true;
		return false;
	}

	bool ExcludeAttribute(const FString& attribute) {
		return HasAttribute(ExcludeAttributes, attribute);
	}

	bool CanIncludeAttributeValues(const FString& attribute) {
		return !HasAttribute(ExcludeAtributesValues, attribute);
	}

	bool IsEnumAttribute(const FString& attribute) {
		return HasAttribute(EnumAttributes, attribute);
	}

	bool IsVectorAttribute(const FString& attribute) {
		return HasAttribute(VectorAttributes, attribute);
	}

	bool IsRelationshipAttribute(const FString& attribute) {
		return HasAttribute(RelationshipAttributes, attribute);
	}

	FString FormatAttributeValue(const Value& value, bool isInnerArray) {
		if (value.IsString())
			return FString::Printf(TEXT("\"%s\""), *FString(UTF8_TO_TCHAR(value.GetString())));
		else if (value.IsNumber()) {
			if (value.IsDouble() || value.IsFloat())
				return FString::SanitizeFloat(value.GetDouble());
			else if (value.IsInt64())
				return FString::Printf(TEXT("%lld"), value.GetInt64());
			else
				return FString::Printf(TEXT("%d"), value.GetInt());
		}
		else if (value.IsBool())
			return value.GetBool() ? TEXT("true") : TEXT("false");
		else if (value.IsArray()) {
			FString result;

			if (isInnerArray) { // Inner array: use double curly braces				
				result += TEXT("{{");
				for (SizeType i = 0; i < value.Size(); ++i) {
					if (i > 0)
						result += TEXT(", ");
					result += FormatAttributeValue(value[i]);
				}
				result += TEXT("}}");
			}
			else { // Outer array: use square brackets				
				result += TEXT("[");
				for (SizeType i = 0; i < value.Size(); ++i) {
					if (i > 0)
						result += TEXT(", ");
					if (value[i].IsArray()) // If this element is an array, format it as inner array
						result += FormatAttributeValue(value[i], true);
					else
						result += FormatAttributeValue(value[i]);
				}
				result += TEXT("]");
			}
			return result;
		}
		else if (value.IsObject())
			return TEXT("\"{object}\"");
		else
			return TEXT("\"UNKNOWN\"");
	}

	FString GetOpacity(const Value& attributes) {
		for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
			FString name = UTF8_TO_TCHAR(itr->name.GetString());
			if (name.Contains(OPACITY_ATTRIBUTE))
				return FormatAttributeValue(itr->value);
		}
		return TEXT("1");
	}

	FString ProcessAttributes(
		const Value& attributes,
		const TArray<FString>& names,
		const TArray<bool>& includeValues,
		const TArray<bool>& enums,
		const TArray<bool>& vectors,
		const TArray<bool>& relationships) {
		FString result;

		for (int32 i = 0; i < names.Num(); ++i) {
			const FString& fullAttrName = names[i];
			if (ExcludeAttribute(fullAttrName)) continue;

			FString owner, attrName;
			if (!fullAttrName.Split(ATTRIBUTE_SEPARATOR, &owner, &attrName)) {
				attrName = fullAttrName;
				owner = TEXT("Unknown");
			}

			FTCHARToUTF8 utf8AttrName(*fullAttrName);
			const char* attrNameUtf8 = utf8AttrName.Get();
			auto memberItr = attributes.FindMember(attrNameUtf8);

			if (memberItr == attributes.MemberEnd()) continue;

			const Value& attrValue = memberItr->value;
			FString name = FormatName(attrName);

			if (!includeValues[i]) {
				result += FString::Printf(TEXT("\t(%s, %s)\n"), *owner, *name);
				continue;
			}

			if (enums[i]) {
				result += FString::Printf(TEXT("\t(%s, %s): {%s}\n"), *owner, *name, UTF8_TO_TCHAR(attrValue.GetString()));
				continue;
			}

			if (vectors[i]) {
				result += FString::Printf(TEXT("\t(%s, %s): {{"), *owner, *name);
				for (SizeType j = 0; j < attrValue.Size(); ++j) {
					if (j > 0)
						result += TEXT(", ");
					result += FormatAttributeValue(attrValue[j]);
				}
				if (name.Equals(DIFFUSECOLOR_COMPONENT))
					result += TEXT(", ") + GetOpacity(attributes);
				result += TEXT("}}\n");
				continue;
			}

			if (relationships[i]) {
				const Value& relObj = attrValue;
				if (relObj.IsObject())
					for (auto relIt = relObj.MemberBegin(); relIt != relObj.MemberEnd(); ++relIt) {
						FString field = UTF8_TO_TCHAR(relIt->name.GetString());
						if (relIt->value.IsObject() && relIt->value.HasMember("ref")) {
							FString refId = UTF8_TO_TCHAR(relIt->value["ref"].GetString());
							FString fullComponent = FString::Printf(TEXT("%s_%s"), *name, *FormatName(field));
							result += FString::Printf(TEXT("\t(%s, %s): {\"%s\"}\n"), *owner, *fullComponent, *refId);
						}
					}
				continue;
			}

			result += FString::Printf(TEXT("\t(%s, %s): {"), *owner, *name);

			if (attrValue.IsObject()) {
				bool first = true;
				for (auto it = attrValue.MemberBegin(); it != attrValue.MemberEnd(); ++it) {
					if (!first)
						result += TEXT(", ");
					first = false;
					result += FormatAttributeValue(it->value);
				}
			}
			else
				result += FormatAttributeValue(attrValue);

			result += TEXT("}\n");
		}

		return result;
	}

	FString GetAttributesOLD(const Value& object) {
		FString result = FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(IFCData)));

		if (!object.HasMember(ATTRIBUTES) || !object[ATTRIBUTES].IsObject())
			return result;

		const Value& attributes = object[ATTRIBUTES];
		TArray<FString> names;
		TArray<bool> enums;
		TArray<bool> vectors;
		TArray<bool> relationships;
		TArray<bool> includeValues;

		for (auto itr = attributes.MemberBegin(); itr != attributes.MemberEnd(); ++itr) {
			FString attrName = UTF8_TO_TCHAR(itr->name.GetString());
			bool isEnum = IsEnumAttribute(attrName);
			bool isVector = IsVectorAttribute(attrName);
			bool isRelationship = IsRelationshipAttribute(attrName);
			bool includeAttributeValues = CanIncludeAttributeValues(attrName) || isEnum || isVector || isRelationship;

			names.Add(attrName);
			enums.Add(isEnum);
			vectors.Add(isVector);
			relationships.Add(isRelationship);
			includeValues.Add(includeAttributeValues);
		}

		return result + ProcessAttributes(attributes, names, includeValues, enums, vectors, relationships);
	}

#pragma endregion

#pragma region Data
	FString GetInheritances(const rapidjson::Value& object, FString owner = "") {
		TArray<FString> inheritIDs;

		if (!owner.IsEmpty())
			inheritIDs.Add(owner);

		if (object.HasMember(INHERITS) && object[INHERITS].IsObject()) {
			const rapidjson::Value& inherits = object[INHERITS];
			for (auto itr = inherits.MemberBegin(); itr != inherits.MemberEnd(); ++itr) {
				const rapidjson::Value& v = itr->value;
				if (v.IsString())
					inheritIDs.Add(UTF8_TO_TCHAR(v.GetString()));
			}
		}

		return inheritIDs.Num() > 0 ? TEXT(": ") + FString::Join(inheritIDs, TEXT(", ")) : TEXT("");
	}

	FString GetChildren(const Value& object, bool isPrefab) {
		if (!object.HasMember(CHILDREN) || !object[CHILDREN].IsObject())
			return TEXT("");

		const FString owner = object[OWNER].GetString();
		const Value& children = object[CHILDREN];
		FString result;

		for (auto itr = children.MemberBegin(); itr != children.MemberEnd(); ++itr) {
			const FString name = FormatName(UTF8_TO_TCHAR(itr->name.GetString()));
			const Value& value = itr->value;

			if (value.IsString()) {
				const FString inheritance = UTF8_TO_TCHAR(value.GetString());

				auto nameComponent = FString::Printf(TEXT("%s: {\"%s\"}"), UTF8_TO_TCHAR(COMPONENT(Name)), *CleanName(name));

				result += FString::Printf(TEXT("\t%s%s: %s, %s {%s}\n"),
					isPrefab ? PREFAB : TEXT(""),
					*name,
					*inheritance,
					*owner,
					*nameComponent);
			}
		}

		return result;
	}

	TArray<const rapidjson::Value*> Sort(const rapidjson::Value& dataArray) {
		TMap<FString, const rapidjson::Value*> objectMap;
		TMap<FString, TArray<FString>> dependencies;

		// Step 1: Build object map and empty dependency list
		for (auto& entry : dataArray.GetArray()) {
			if (!entry.HasMember(PATH) || !entry[PATH].IsString())
				continue;

			FString id = UTF8_TO_TCHAR(entry[PATH].GetString());
			objectMap.Add(id, &entry);
			dependencies.Add(id, {});
		}

		// Step 2: Fill in dependencies
		for (auto& entry : dataArray.GetArray()) {
			if (!entry.HasMember(PATH) || !entry[PATH].IsString())
				continue;

			FString id = UTF8_TO_TCHAR(entry[PATH].GetString());

			if (entry.HasMember(CHILDREN) && entry[CHILDREN].IsObject())
				for (auto& child : entry[CHILDREN].GetObject()) {
					FString childId = UTF8_TO_TCHAR(child.value.GetString());
					if (dependencies.Contains(id))
						dependencies[id].Add(childId); // id depends on child
				}

			if (entry.HasMember(INHERITS) && entry[INHERITS].IsObject())
				for (auto& inherit : entry[INHERITS].GetObject()) {
					FString baseId = UTF8_TO_TCHAR(inherit.value.GetString());
					if (dependencies.Contains(id))
						dependencies[id].Add(baseId); // id depends on base
				}
		}

		// Step 3: List of all UUIDs to sort
		TArray<FString> sortedIds;
		dependencies.GetKeys(sortedIds);

		// Step 4: Sort them using correct lambda: return array of dependencies for given node
		bool success = Algo::TopologicalSort(sortedIds, [&dependencies](const FString& id) -> const TArray<FString>&{
			return dependencies[id]; // id depends on these
			});

		if (!success)
			UE_LOG(LogTemp, Warning, TEXT(">>> Cyclic dependency detected in prefab graph."));

		// Step 5: Convert back to JSON pointers
		TArray<const rapidjson::Value*> sortedObjects;
		for (const FString& id : sortedIds)
			if (const rapidjson::Value** value = objectMap.Find(id))
				sortedObjects.Add(*value);

		return sortedObjects;
	}

	void MergeObjectMembers(Value& target, const Value& source, const char* memberName, Document::AllocatorType& allocator) {
		if (!source.HasMember(memberName) || !source[memberName].IsObject())
			return;

		if (!target.HasMember(memberName))
			target.AddMember(Value(memberName, allocator), Value(kObjectType), allocator);

		Value& targetObject = target[memberName];
		const Value& sourceObject = source[memberName];

		for (auto itr = sourceObject.MemberBegin(); itr != sourceObject.MemberEnd(); ++itr) {
			Value key(itr->name, allocator);
			Value value(itr->value, allocator);
			targetObject.RemoveMember(key);
			targetObject.AddMember(key, value, allocator);
		}
	}

	Value Merge(const Value& inputArray, Document::AllocatorType& allocator) {
		Value mergedArray(kArrayType);
		TMap<FString, Value> mergedObjects;

		for (SizeType i = 0; i < inputArray.Size(); ++i) {
			const Value& object = inputArray[i];
			if (!object.IsObject() || !object.HasMember(PATH) || !object[PATH].IsString())
				continue;

			FString pathStr = UTF8_TO_TCHAR(object[PATH].GetString());

			if (!mergedObjects.Contains(pathStr)) {
				Value newObj(kObjectType);
				newObj.CopyFrom(object, allocator);
				mergedObjects.Add(pathStr, MoveTemp(newObj));
			}
			else {
				Value& existing = mergedObjects[pathStr];
				MergeObjectMembers(existing, object, INHERITS, allocator);
				MergeObjectMembers(existing, object, ATTRIBUTES, allocator);
				MergeObjectMembers(existing, object, CHILDREN, allocator);
			}
		}

		for (const auto& Pair : mergedObjects) {
			Value copy(Pair.Value, allocator);
			mergedArray.PushBack(copy, allocator);
		}

		return mergedArray;
	}

	FString ParseData(const rapidjson::Value& data, rapidjson::Document::AllocatorType& allocator) {
		rapidjson::Value merged = Merge(data, allocator);
		TArray<const rapidjson::Value*> sorted = Sort(merged);

		TSet<FString> entities; // Find entities: non repeating ID
		for (const rapidjson::Value* object : sorted) {
			if (object && object->IsObject())
				entities.Add(UTF8_TO_TCHAR((*object)[PATH].GetString()));
			if ((*object).HasMember(CHILDREN) && (*object)[CHILDREN].IsObject())
				for (auto& child : (*object)[CHILDREN].GetObject())
					entities.Remove(UTF8_TO_TCHAR(child.value.GetString()));
			if ((*object).HasMember(INHERITS) && (*object)[INHERITS].IsObject())
				for (auto& inherit : (*object)[INHERITS].GetObject())
					entities.Remove(UTF8_TO_TCHAR(inherit.value.GetString()));
		}

		FString result;
		for (const rapidjson::Value* object : sorted) {
			if (!object || !object->IsObject()) 
				continue;

			FString path = UTF8_TO_TCHAR((*object)[PATH].GetString());
			bool isPrefab = !entities.Contains(path);

			FString components = FString::Printf(TEXT("\t%s\n"), ECS::OrderedChildrenTrait);
			components += FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(IFCData)));
			if (!isPrefab)
				components += FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(Hierarchy)));

			const rapidjson::Value& value = *object;
			const FString owner = value[OWNER].GetString();

			result += FString::Printf(TEXT("%s%s.%s%s {\n%s%s%s}\n"),
				isPrefab ? PREFAB : TEXT(""),
				*IFC::Scope(),
				*path,
				*GetInheritances(*object, isPrefab ? TEXT("") : *owner),
				*components,
				*GetChildren(*object, isPrefab),
				*GetAttributes(*object));
		}
		return result;
	}
#pragma endregion

	void InjectOwner(rapidjson::Value& object, const FString& layerPath, rapidjson::Document::AllocatorType& allocator) {
		auto ownerPath = GetOwnerPath(layerPath);
		if (!object.HasMember(ATTRIBUTES) || !object[ATTRIBUTES].IsObject()) {
			object.AddMember(rapidjson::Value(OWNER, allocator),
				rapidjson::Value(TCHAR_TO_UTF8(*ownerPath), allocator),
				allocator);
			return;
		}

		Value& attributes = object[ATTRIBUTES];
		TArray<Value> keys;
		TArray<Value> values;

		for (auto it = attributes.MemberBegin(); it != attributes.MemberEnd(); ++it) {
			const FString originalKey = UTF8_TO_TCHAR(it->name.GetString());
			const FString prefixedKey = ownerPath + ATTRIBUTE_SEPARATOR + originalKey;
			keys.Add(Value(TCHAR_TO_UTF8(*prefixedKey), allocator));
			values.Add(Value(it->value, allocator));
		}

		attributes.RemoveAllMembers();

		for (int32 i = 0; i < keys.Num(); ++i)
			attributes.AddMember(MoveTemp(keys[i]), MoveTemp(values[i]), allocator);
	}

	void LoadIFCData(flecs::world& world, const TArray<flecs::entity> layers) {
		rapidjson::Document tempDoc;
		rapidjson::Document::AllocatorType& allocator = tempDoc.GetAllocator();
		rapidjson::Value combinedData(rapidjson::kArrayType);
		
		FString code = FString::Printf(TEXT("using %s\n"), *Scope());

		FString layerNames;

		for (const flecs::entity layer : layers) {
			FString filePath = layer.try_get<Path>()->Value;
			auto jsonString = Assets::LoadTextFile(filePath);
			auto formatted = FormatUUID(jsonString);
			free(jsonString);

			rapidjson::Document doc;
			if (doc.Parse(TCHAR_TO_UTF8(*formatted)).HasParseError()) {
				UE_LOG(LogTemp, Error, TEXT(">>> Parse error in file %s: %s"), *filePath, *FString(GetParseError_En(doc.GetParseError())));
				continue;
			}

			if (!doc.HasMember(HEADER) || !doc[HEADER].IsObject()) {
				UE_LOG(LogTemp, Warning, TEXT(">>> Invalid Header: %s"), *filePath);
				continue;
			}

			if (!doc.HasMember(DATA) || !doc[DATA].IsArray()) {
				UE_LOG(LogTemp, Warning, TEXT(">>> Invalid Data: %s"), *filePath);
				continue;
			}

			for (auto& entry : doc[DATA].GetArray()) {
				rapidjson::Value copy(entry, allocator);
				InjectOwner(copy, ECS::NormalizedPath(layer.path().c_str()), allocator);
				combinedData.PushBack(copy, allocator);
			}

			layerNames += layer.try_get<Id>()->Value + " | ";
		}

		code += ParseData(combinedData, allocator);
		ECS::RunCode(world, layerNames, code);
	}
}