#include "MaterialSubsystem.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"
#include "Hash/CityHash.h"

static const FName baseColorParamName("Base Color");

uint64 UMaterialSubsystem::MakeHash(UMaterialInterface* master, const FVector4f& rgba, bool opaque) {
	uint64 h = 1469598103934665603ull;
	h ^= reinterpret_cast<uint64>(master) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
	h ^= (opaque ? 1ull : 0ull) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
	const uint64 colorHash = CityHash64(reinterpret_cast<const char*>(&rgba), sizeof(FVector4f));
	h ^= colorHash + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
	return h;
}

int32 UMaterialSubsystem::CreateMaterial(UWorld* world, const FVector4f& rgba) {
	const bool opaque = rgba.W > 0.99f;
	UMaterialInterface* master = opaque ? MOpaque : MTranslucent;
	const uint64 h = MakeHash(master, rgba, opaque);
	if (const int32* found = HashToId.Find(h)) {
		Retain(*found);
		return *found;
	}
	UMaterialInstanceDynamic* mid = UMaterialInstanceDynamic::Create(master, world);
	mid->SetVectorParameterValue(baseColorParamName, FLinearColor(rgba.X, rgba.Y, rgba.Z, rgba.W));
	return Register(mid, h);
}

int32 UMaterialSubsystem::Register(UMaterialInstanceDynamic* mid, uint64 contentHash) {
	if (!mid) return INDEX_NONE;
	if (contentHash != 0) {
		if (const int32* found = HashToId.Find(contentHash)) {
			MaterialEntryData* entry = EntryData.Find(*found);
			if (!entry) return INDEX_NONE;
			++entry->RefCount;
			entry->LastAccess = FPlatformTime::Seconds();
			return *found;
		}
	}
	const int32 newId = NextId++;
	Materials.Add(newId, mid);
	MaterialEntryData& data = EntryData.Add(newId);
	data.RefCount = 1;
	data.ContentHash = contentHash;
	data.LastAccess = FPlatformTime::Seconds();
	if (contentHash != 0) HashToId.Add(contentHash, newId);
	return newId;
}

void UMaterialSubsystem::Retain(int32 id) {
	MaterialEntryData* entry = EntryData.Find(id);
	if (!entry) return;
	++entry->RefCount;
	entry->LastAccess = FPlatformTime::Seconds();
}

void UMaterialSubsystem::Release(int32 id) {
	MaterialEntryData* entry = EntryData.Find(id);
	if (!entry) return;
	--entry->RefCount;
	if (entry->RefCount > 0) {
		entry->LastAccess = FPlatformTime::Seconds();
		return;
	}
	const uint64 hash = entry->ContentHash;
	EntryData.Remove(id);
	UMaterialInstanceDynamic* mid = nullptr;
	if (const TObjectPtr<UMaterialInstanceDynamic>* ptr = Materials.Find(id)) mid = ptr->Get();
	Materials.Remove(id);
	if (hash != 0) HashToId.Remove(hash);
	if (mid) mid->MarkAsGarbage();
}

UMaterialInstanceDynamic* UMaterialSubsystem::Get(int32 id) const {
	const TObjectPtr<UMaterialInstanceDynamic>* ptr = Materials.Find(id);
	return ptr ? ptr->Get() : nullptr;
}