// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFC.h"
#include "LayerFeature.h"
#include "AttributeFeature.h"
#include "ModelFeature.h"
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
		using namespace ECS;

		world.component<Name>().member<FString>(VALUE).add(flecs::OnInstantiate, flecs::Inherit);
		world.component<IfcObject>().add(flecs::OnInstantiate, flecs::Inherit);

		world.component<Root>();
		world.component<Branch>().add(flecs::OnInstantiate, flecs::Inherit);

		world.component<QueryIfcData>();
		world.set(QueryIfcData{
			world.query_builder<>(COMPONENT(QueryIfcData))
			.with<IfcObject>()
			.with(flecs::Prefab).optional()
			.cached().build() });

		LayerFeature::CreateComponents(world);
		AttributeFeature::CreateComponents(world);
		ModelFeature::CreateComponents(world);

		LayerFeature::CreateQueries(world);

		ModelFeature::CreateObservers(world);

		AttributeFeature::Initialize(world);
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

	FString ToUUID(const FString& input) {
		FString output = input;
		const FRegexPattern idPattern(TEXT(R"(ID([a-fA-F0-9]{32}))"));
		FRegexMatcher matcher(idPattern, output);

		while (matcher.FindNext()) {
			const FString fullMatch = matcher.GetCaptureGroup(0);
			const FString rawUuid = matcher.GetCaptureGroup(1);
			const FString dashed = FString::Printf(
				TEXT("%s-%s-%s-%s-%s"),
				*rawUuid.Mid(0, 8),
				*rawUuid.Mid(8, 4),
				*rawUuid.Mid(12, 4),
				*rawUuid.Mid(16, 4),
				*rawUuid.Mid(20, 12)
			);

			output = output.Replace(*fullMatch, *dashed, ESearchCase::IgnoreCase);
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

	FString GetInheritances(const rapidjson::Value& object, const FString& owner) {
		TArray<FString> inheritIDs;

		if (!owner.IsEmpty())
			inheritIDs.Add(owner);

		if (object.HasMember(INHERITS_KEY) && object[INHERITS_KEY].IsObject()) {
			const rapidjson::Value& inherits = object[INHERITS_KEY];
			for (auto inherit = inherits.MemberBegin(); inherit != inherits.MemberEnd(); ++inherit) {
				FString inheritance = IFC::Scope() + "." + UTF8_TO_TCHAR(inherit->value.GetString());
				inheritIDs.Add(inheritance);
			}
		}

		return inheritIDs.Num() > 0 ? TEXT(": ") + FString::Join(inheritIDs, TEXT(", ")) : TEXT("");
	}

	FString GetChildren(const rapidjson::Value& object, bool isPrefab) {
		if (!object.HasMember(CHILDREN_KEY) || !object[CHILDREN_KEY].IsObject())
			return TEXT("");

		const FString owner = object[OWNER].GetString();
		const rapidjson::Value& children = object[CHILDREN_KEY];

		FString result;

		if (isPrefab) {
			result += FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(Branch)));
			result += FString::Printf(TEXT("\t%s\n"), ECS::OrderedChildrenTrait);
		}

		for (auto child = children.MemberBegin(); child != children.MemberEnd(); ++child) {
			const FString name = FormatName(UTF8_TO_TCHAR(child->name.GetString()));
			auto nameComponent = FString::Printf(TEXT("%s: {\"%s\"}"), UTF8_TO_TCHAR(COMPONENT(Name)), *CleanName(name));

			FString inheritance = IFC::Scope() + "." + UTF8_TO_TCHAR(child->value.GetString());

			result += FString::Printf(TEXT("\t%s%s: %s, %s {%s}\n"),
				isPrefab ? PREFAB : TEXT(""),
				*name,
				*inheritance,
				*owner,
				*nameComponent);
		}

		return result;
	}

	TArray<const rapidjson::Value*> Sort(const rapidjson::Value& dataArray) {
		TMap<FString, const rapidjson::Value*> objectMap;
		TMap<FString, TArray<FString>> dependencies;

		// Step 1: Build object map and empty dependency list
		for (auto& entry : dataArray.GetArray()) {
			if (!entry.HasMember(PATH_KEY) || !entry[PATH_KEY].IsString())
				continue;

			FString id = UTF8_TO_TCHAR(entry[PATH_KEY].GetString());
			objectMap.Add(id, &entry);
			dependencies.Add(id, {});
		}

		// Step 2: Fill in dependencies
		for (auto& entry : dataArray.GetArray()) {
			if (!entry.HasMember(PATH_KEY) || !entry[PATH_KEY].IsString())
				continue;

			FString id = UTF8_TO_TCHAR(entry[PATH_KEY].GetString());

			if (entry.HasMember(CHILDREN_KEY) && entry[CHILDREN_KEY].IsObject())
				for (auto& child : entry[CHILDREN_KEY].GetObject()) {
					FString childId = UTF8_TO_TCHAR(child.value.GetString());
					if (dependencies.Contains(id))
						dependencies[id].Add(childId); // id depends on child
				}

			if (entry.HasMember(INHERITS_KEY) && entry[INHERITS_KEY].IsObject())
				for (auto& inherit : entry[INHERITS_KEY].GetObject()) {
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

	void MergeObjectMembers (rapidjson::Value& target, const rapidjson::Value& source, const char* memberName, Document::AllocatorType& allocator) {
		if (!source.HasMember(memberName) || !source[memberName].IsObject())
			return;

		if (!target.HasMember(memberName))
			target.AddMember(rapidjson::Value(memberName, allocator), rapidjson::Value(kObjectType), allocator);

		rapidjson::Value& targetObject = target[memberName];
		const rapidjson::Value& sourceObject = source[memberName];

		for (auto member = sourceObject.MemberBegin(); member != sourceObject.MemberEnd(); ++member) {
			rapidjson::Value key(member->name, allocator);
			rapidjson::Value value(member->value, allocator);
			targetObject.RemoveMember(key);
			targetObject.AddMember(key, value, allocator);
		}
	}

	rapidjson::Value Merge(const rapidjson::Value& inputArray, Document::AllocatorType& allocator) {
		rapidjson::Value mergedArray(kArrayType);
		TMap<FString, rapidjson::Value> mergedObjects;

		for (SizeType i = 0; i < inputArray.Size(); ++i) {
			const rapidjson::Value& object = inputArray[i];
			if (!object.IsObject() || !object.HasMember(PATH_KEY) || !object[PATH_KEY].IsString())
				continue;

			FString pathStr = UTF8_TO_TCHAR(object[PATH_KEY].GetString());

			if (!mergedObjects.Contains(pathStr)) {
				rapidjson::Value newObj(kObjectType);
				newObj.CopyFrom(object, allocator);
				mergedObjects.Add(pathStr, MoveTemp(newObj));
			}
			else {
				rapidjson::Value& existing = mergedObjects[pathStr];
				MergeObjectMembers(existing, object, INHERITS_KEY, allocator);
				MergeObjectMembers(existing, object, ATTRIBUTES_KEY, allocator);
				MergeObjectMembers(existing, object, CHILDREN_KEY, allocator);
			}
		}

		for (const auto& Pair : mergedObjects) {
			rapidjson::Value copy(Pair.Value, allocator);
			mergedArray.PushBack(copy, allocator);
		}

		return mergedArray;
	}

	FString ParseData(flecs::world& world, const rapidjson::Value& data, rapidjson::Document::AllocatorType& allocator) {
		rapidjson::Value merged = Merge(data, allocator);
		TArray<const rapidjson::Value*> sorted = Sort(merged);

		TSet<FString> entities; // Find entities: non repeating ID
		for (const rapidjson::Value* object : sorted) {
			if (object && object->IsObject())
				entities.Add(UTF8_TO_TCHAR((*object)[PATH_KEY].GetString()));
			if ((*object).HasMember(CHILDREN_KEY) && (*object)[CHILDREN_KEY].IsObject())
				for (auto& child : (*object)[CHILDREN_KEY].GetObject())
					entities.Remove(UTF8_TO_TCHAR(child.value.GetString()));
			if ((*object).HasMember(INHERITS_KEY) && (*object)[INHERITS_KEY].IsObject())
				for (auto& inherit : (*object)[INHERITS_KEY].GetObject())
					entities.Remove(UTF8_TO_TCHAR(inherit.value.GetString()));
		}

		FString result;
		FString attributeRelationship = ECS::NormalizedPath(world.try_get<AttributeRelationship>()->Value.path().c_str());

		for (const rapidjson::Value* object : sorted) {
			if (!object || !object->IsObject()) 
				continue;

			FString path = UTF8_TO_TCHAR((*object)[PATH_KEY].GetString());
			bool isPrefab = !entities.Contains(path);

			TTuple<FString, FString> attributes = GetAttributes(world, *object, *path);

			result += attributes.Get<1>();

			FString components = FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(IfcObject)));
			if (isPrefab)
				components += FString::Printf(TEXT("\t(%s, %s)\n"), *attributeRelationship, *attributes.Get<0>());
			else {
				components += FString::Printf(TEXT("\t%s\n"), UTF8_TO_TCHAR(COMPONENT(Root)));
				components += FString::Printf(TEXT("\t%s: {\"%s\"}\n"), UTF8_TO_TCHAR(COMPONENT(Name)), *ToUUID(path));
			}

			const FString owner = (*object)[OWNER].GetString();

			result += FString::Printf(TEXT("%s%s.%s%s {\n%s%s}\n"),
				isPrefab ? PREFAB : TEXT(""),
				*IFC::Scope(),
				*path,
				*GetInheritances(*object, isPrefab ? TEXT("") : *owner),
				*components,
				*GetChildren(*object, isPrefab));
		}
		return result;
	}

	void InjectOwner(rapidjson::Value& object, const FString& layerPath, rapidjson::Document::AllocatorType& allocator) {
		auto ownerPath = GetOwnerPath(layerPath);
		if (!object.HasMember(ATTRIBUTES_KEY) || !object[ATTRIBUTES_KEY].IsObject()) {
			object.AddMember(rapidjson::Value(OWNER, allocator),
				rapidjson::Value(TCHAR_TO_UTF8(*ownerPath), allocator),
				allocator);
			return;
		}

		rapidjson::Value& attributes = object[ATTRIBUTES_KEY];
		TArray<rapidjson::Value> keys;
		TArray<rapidjson::Value> values;

		for (auto it = attributes.MemberBegin(); it != attributes.MemberEnd(); ++it) {
			const FString originalKey = UTF8_TO_TCHAR(it->name.GetString());
			const FString prefixedKey = ownerPath + ATTRIBUTE_SEPARATOR + originalKey;
			keys.Add(rapidjson::Value(TCHAR_TO_UTF8(*prefixedKey), allocator));
			values.Add(rapidjson::Value(it->value, allocator));
		}

		attributes.RemoveAllMembers();

		for (int32 i = 0; i < keys.Num(); ++i)
			attributes.AddMember(MoveTemp(keys[i]), MoveTemp(values[i]), allocator);
	}

	void LoadIfcData(flecs::world& world, const TArray<flecs::entity> layers) {
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

			if (!doc.HasMember(DATA_KEY) || !doc[DATA_KEY].IsArray()) {
				UE_LOG(LogTemp, Warning, TEXT(">>> Invalid Data: %s"), *filePath);
				continue;
			}

			for (auto& entry : doc[DATA_KEY].GetArray()) {
				rapidjson::Value copy(entry, allocator);
				InjectOwner(copy, ECS::NormalizedPath(layer.path().c_str()), allocator);
				combinedData.PushBack(copy, allocator);
			}

			layerNames += layer.try_get<Id>()->Value + " | ";
		}

		code += ParseData(world, combinedData, allocator);
		ECS::RunCode(world, layerNames, code);
	}
}