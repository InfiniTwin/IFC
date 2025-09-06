// Fill out your copyright notice in the Description page of Project Settings.

#include "MeshSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

static uint64 ComputeContentHash(const TArray<FVector3f>& points, const TArray<int32>& indices) {
    TArray<uint8> buffer;
    buffer.Reserve(points.Num() * sizeof(FVector3f) + indices.Num() * sizeof(int32));
    if (points.Num() > 0) buffer.Append(reinterpret_cast<const uint8*>(points.GetData()), points.Num() * sizeof(FVector3f));
    if (indices.Num() > 0) buffer.Append(reinterpret_cast<const uint8*>(indices.GetData()), indices.Num() * sizeof(int32));
    return FXxHash64::HashBuffer(buffer.GetData(), buffer.Num()).Hash;
}

int32 UMeshSubsystem::CreateMesh(UWorld* world, const TArray<FVector3f>& points, const TArray<int32>& indices) {
    if (!world) return INDEX_NONE;
    if (points.Num() == 0) return INDEX_NONE;
    if (indices.Num() == 0 || (indices.Num() % 3) != 0) return INDEX_NONE;

    UStaticMesh* mesh = NewObject<UStaticMesh>(this, NAME_None, RF_Transient);
    if (!mesh) return INDEX_NONE;

    mesh->SetMinLOD(0);

    FMeshDescription meshDescription;
    FStaticMeshAttributes attributes(meshDescription);
    attributes.Register();

    TVertexAttributesRef<FVector3f> vertexPositions = attributes.GetVertexPositions();

    TArray<FVertexID> vertexIds;

    TArray<FVector3f> scaledPoints = points;
    for (FVector3f& point : scaledPoints) point *= 100.f;

    vertexIds.Reserve(scaledPoints.Num());
    for (const FVector3f& position : scaledPoints) {
        FVertexID vertexId = meshDescription.CreateVertex();
        vertexPositions[vertexId] = position;
        vertexIds.Add(vertexId);
    }

    FPolygonGroupID polygonGroupId = meshDescription.CreatePolygonGroup();

    for (int32 i = 0; i < indices.Num(); i += 3) {
        int32 i0 = indices[i + 0];
        int32 i1 = indices[i + 1];
        int32 i2 = indices[i + 2];
        if (!vertexIds.IsValidIndex(i0) || !vertexIds.IsValidIndex(i1) || !vertexIds.IsValidIndex(i2)) continue;

        FVertexInstanceID vi0 = meshDescription.CreateVertexInstance(vertexIds[i0]);
        FVertexInstanceID vi1 = meshDescription.CreateVertexInstance(vertexIds[i1]);
        FVertexInstanceID vi2 = meshDescription.CreateVertexInstance(vertexIds[i2]);

        TArray<FVertexInstanceID> triangle;
        triangle.Add(vi0);
        triangle.Add(vi1);
        triangle.Add(vi2);

        meshDescription.CreatePolygon(polygonGroupId, triangle);
    }

    FStaticMeshOperations::ComputeTriangleTangentsAndNormals(meshDescription);

    UStaticMesh::FBuildMeshDescriptionsParams buildParams;
    buildParams.bAllowCpuAccess = true;
    buildParams.bBuildSimpleCollision = false;
    buildParams.bCommitMeshDescription = false;
    buildParams.bFastBuild = true;

    TArray<const FMeshDescription*> meshDescriptions;
    meshDescriptions.Add(&meshDescription);
    if (!mesh->BuildFromMeshDescriptions(meshDescriptions, buildParams)) return INDEX_NONE;

    mesh->InitResources();
    mesh->CalculateExtendedBounds();

    uint64 hash = ComputeContentHash(points, indices);
    return RegisterMesh(mesh, hash);
}

int32 UMeshSubsystem::RegisterMesh(UStaticMesh* mesh, uint64 contentHash) {
    if (!mesh) return INDEX_NONE;
    int32 existingId = INDEX_NONE;
    if (contentHash != 0) {
        const int32* foundId = HashToId.Find(contentHash);
        if (foundId) existingId = *foundId;
    }
    if (existingId != INDEX_NONE) {
        MeshEntryData* entry = EntryData.Find(existingId);
        if (!entry) return INDEX_NONE;
        ++entry->RefCount;
        entry->LastAccess = FPlatformTime::Seconds();
        return existingId;
    }
    int32 newId = NextId++;
    Meshes.Add(newId, mesh);
    MeshEntryData& newEntry = EntryData.Add(newId);
    newEntry.RefCount = 1;
    newEntry.ContentHash = contentHash;
    newEntry.LastAccess = FPlatformTime::Seconds();
    if (contentHash != 0) HashToId.Add(contentHash, newId);
    return newId;
}

bool UMeshSubsystem::TryFindByHash(uint64 contentHash, int32& outId) const {
    if (contentHash == 0) return false;
    const int32* foundId = HashToId.Find(contentHash);
    if (!foundId) return false;
    outId = *foundId;
    return true;
}

void UMeshSubsystem::Retain(int32 id) {
    MeshEntryData* entry = EntryData.Find(id);
    if (!entry) return;
    ++entry->RefCount;
    entry->LastAccess = FPlatformTime::Seconds();
}

void UMeshSubsystem::Release(int32 id, bool destroyNow) {
    MeshEntryData* entry = EntryData.Find(id);
    if (!entry) return;
    --entry->RefCount;
    if (entry->RefCount > 0) {
        entry->LastAccess = FPlatformTime::Seconds();
        return;
    }
    uint64 hash = entry->ContentHash;
    EntryData.Remove(id);
    UStaticMesh* mesh = nullptr;
    if (const TObjectPtr<UStaticMesh>* meshPtr = Meshes.Find(id)) mesh = meshPtr->Get();
    Meshes.Remove(id);
    if (hash != 0) HashToId.Remove(hash);
    if (destroyNow && mesh) mesh->MarkAsGarbage();
}

UStaticMesh* UMeshSubsystem::Get(int32 id) const {
    const TObjectPtr<UStaticMesh>* meshPtr = Meshes.Find(id);
    if (!meshPtr) return nullptr;
    return meshPtr->Get();
}

void UMeshSubsystem::Touch(int32 id) {
    MeshEntryData* entry = EntryData.Find(id);
    if (!entry) return;
    entry->LastAccess = FPlatformTime::Seconds();
}

MeshStats UMeshSubsystem::GetStats() const {
    MeshStats stats;
    stats.Count = EntryData.Num();
    int32 totalRefCount = 0;
    for (const auto& entryPair : EntryData) totalRefCount += entryPair.Value.RefCount;
    stats.TotalRefCount = totalRefCount;
    return stats;
}