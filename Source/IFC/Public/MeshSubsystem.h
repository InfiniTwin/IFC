// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectPtr.h"
#include "Engine/StaticMesh.h"
#include "MeshSubsystem.generated.h"

struct MeshEntryData {
    int32 RefCount = 0;
    uint64 ContentHash = 0;
    double LastAccess = 0.0;
};

struct MeshStats {
    int32 Count = 0;
    int32 TotalRefCount = 0;
};

UCLASS()
class UMeshSubsystem : public UWorldSubsystem {
    GENERATED_BODY()

public:
    int32 CreateMesh(UWorld* world, const TArray<FVector3f>& points, const TArray<int32>& indices); 
    int32 RegisterMesh(UStaticMesh* mesh, uint64 contentHash);
    bool TryFindByHash(uint64 contentHash, int32& outId) const;
    void Retain(int32 id);
    void Release(int32 id, bool destroyNow = false);
    UStaticMesh* Get(int32 id) const;
    void Touch(int32 id);
    MeshStats GetStats() const;

private:
    UPROPERTY() TMap<int32, TObjectPtr<UStaticMesh>> Meshes;
    TMap<int32, MeshEntryData> EntryData;
    TMap<uint64, int32> HashToId;
    int32 NextId = 1;
};