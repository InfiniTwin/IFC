// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MaterialSubsystem.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;

struct MaterialEntryData {
    int32 RefCount = 0;
    uint64 ContentHash = 0;
    double LastAccess = 0.0;
};

struct MaterialStats {
    int32 Count = 0;
    int32 TotalRefCount = 0;
};

UCLASS()
class UMaterialSubsystem : public UWorldSubsystem {
    GENERATED_BODY()

public:
    int32 CreateMaterial(UWorld* world, const FVector4f& rgba);
    static int32 GetRequiredCustomDataFloats() { return 4; }
    void Retain(int32 id);
    void Release(int32 id, bool destroyNow);
    UMaterialInterface* Get(int32 id) const;
    void Touch(int32 id);
    MaterialStats GetStats() const;

private:
    static uint64 MakeHash(UMaterialInterface* master, const FVector4f& rgba, bool opaque);
    static UMaterialInterface* LoadMaterialByPath(const TCHAR* path);
    int32 Register(UMaterialInstanceDynamic* mid, uint64 contentHash);

private:
    const TCHAR* MaterialOpaquePath = TEXT("/Game/Materials/Opaque.Opaque");
    const TCHAR* MaterialTranslucentPath = TEXT("/Game/Materials/Translucent.Translucent");
    UMaterialInterface* MOpaque = nullptr;
    UMaterialInterface* MTranslucent = nullptr;

    TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> Materials;
    TMap<int32, MaterialEntryData> EntryData;
    TMap<uint64, int32> HashToId;
    int32 NextId = 1;
};
